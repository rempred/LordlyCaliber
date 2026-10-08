#include "app/formation_board.h"
#include "engine/project.h"
#include <QApplication>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <algorithm>

namespace lordly {
namespace {
// Real buttons retain keyboard focus, activation and accessible names. The board
// paints their shared artwork in depth order; their masks follow the tile tops.
class BoardCell final : public QPushButton {
  public:
    using QPushButton::QPushButton;
  protected:
    void paintEvent(QPaintEvent *) override {}
};
}
FormationBoard::FormationBoard(Project *project, const QString &prefix, bool player,
                               QWidget *parent, int firstCellNumber)
    : QWidget(parent), project_(project), playerFormation_(player) {
    setObjectName(prefix + "Board");
    setFixedSize(sizeHint());
    setMouseTracking(true);
    try { board_ = ArmyArtwork::boardImage(project_->rom()); }
    catch (const std::exception &e) { artworkError_ = QString::fromUtf8(e.what()); }
    for (int i = 0; i < 9; ++i) {
        auto button = new BoardCell(this);
        button->setObjectName(prefix + QString::number(i + firstCellNumber));
        button->setCheckable(true);
        button->setMouseTracking(true);
        button->installEventFilter(this);
        connect(button, &QPushButton::toggled, this, qOverload<>(&QWidget::update));
        cells_[i] = button;
    }
    clearUnits();
    arrangeCells();
}
void FormationBoard::setPixelScale(int scale) {
    zoom_ = std::clamp(scale, 1, 6);
    setFixedSize(sizeHint());
    arrangeCells();
    update();
}
QPointF FormationBoard::origin() const { return {4.0 * zoom_, 20.0 * zoom_}; }
QPointF FormationBoard::foot(int cell) const {
    // Keep the accepted front-row foot offset on every tile. A separate native
    // sprite-grid projection left middle/rear units above this board's tile tops.
    return QPointF(cellCenter(cell)) + QPointF(-zoom_, zoom_);
}
QPolygonF FormationBoard::cellPolygon(int id) const {
    const auto center = QPointF(cellCenter(id));
    // Exclude the beveled risers and transparent corners from highlights and drops.
    return {center + QPointF(-13, -2) * zoom_, center + QPointF(4, -8) * zoom_,
            center + QPointF(13, 2) * zoom_, center + QPointF(-4, 8) * zoom_};
}
QPoint FormationBoard::cellCenter(int id) const {
    // Army Management uses the front-facing board for both player saves and
    // enemy editors. Stored cells 6..8 are the front row in both formats.
    const int display = id;
    return (origin() + QPointF(53 - 19*(display%3) + 10*(display/3),
                               13 + 6*(display%3) + 13*(display/3)) * zoom_).toPoint();
}
int FormationBoard::cellAt(QPoint p) const {
    for (int i = 0; i < 9; ++i)
        if (cellPolygon(i).containsPoint(p, Qt::OddEvenFill)) return i;
    return -1;
}
QRectF FormationBoard::spriteRect(int cell) const {
    const auto size = sprites_[cell].size() * zoom_;
    return {foot(cell) - QPointF(size.width() / 2.0, size.height()), QSizeF(size)};
}
int FormationBoard::memberAt(QPoint p) const {
    int result = -1;
    for (int i = 0; i < 9; ++i) {
        if (!occupied_[i] || sprites_[i].isNull()) continue;
        const auto rect = spriteRect(i);
        const auto local = ((QPointF(p) - rect.topLeft()) / zoom_).toPoint();
        if (rect.contains(p) && sprites_[i].rect().contains(local) &&
            qAlpha(sprites_[i].pixel(local)) && (result < 0 || foot(i).y() > foot(result).y()))
            result = i;
    }
    return result;
}
void FormationBoard::arrangeCells() {
    for (int i = 0; i < 9; ++i) {
        const auto polygon = cellPolygon(i);
        const auto bounds = polygon.boundingRect().toAlignedRect();
        cells_[i]->setGeometry(bounds);
        cells_[i]->setMask(QRegion(polygon.translated(-bounds.topLeft()).toPolygon()));
    }
}
void FormationBoard::resizeEvent(QResizeEvent *) { arrangeCells(); }
void FormationBoard::clearUnits() {
    for (int i = 0; i < 9; ++i) {
        sprites_[i] = {}; occupied_[i] = false; conflicts_[i] = false; hints_[i].clear();
        cells_[i]->setIcon({}); cells_[i]->setChecked(false);
        cells_[i]->setText(QString("Cell %1 · Empty").arg(i + 1));
        cells_[i]->setAccessibleName(cells_[i]->text());
        cells_[i]->setToolTip(cells_[i]->text());
    }
}
void FormationBoard::setUnit(int cell, int classId, const QString &label, bool selected,
                            bool conflict) {
    if (cell < 0 || cell >= 9) return;
    occupied_[cell] = true; conflicts_[cell] = conflict;
    cells_[cell]->setText(label); cells_[cell]->setToolTip(label + QString("\nCell %1").arg(cell+1));
    cells_[cell]->setAccessibleName(QString("Cell %1 · %2").arg(cell + 1).arg(label));
    cells_[cell]->setChecked(selected);
    try {
        project_->loadArmyArtwork();
        const auto classes = project_->classData();
        if (classId < 0 || classId >= classes.entries().size()) throw Error("Class has no Army sprite route");
        const auto &asset = classes.entries().at(classId);
        const int lane = ArmyArtwork::classLane(asset), model = ArmyArtwork::classModel(asset);
        sprites_[cell] = project_->armyArtwork()->image(lane * 2, model);
        cells_[cell]->setIcon(QPixmap::fromImage(sprites_[cell]));
    } catch (const std::exception &e) {
        cells_[cell]->setToolTip(label + "\nArtwork unavailable: " + QString::fromUtf8(e.what()));
    }
}
void FormationBoard::setCellHint(int cell, const QString &hint) {
    if (cell < 0 || cell >= 9 || occupied_[cell]) return;
    hints_[cell] = hint;
    cells_[cell]->setToolTip(hint);
}
void FormationBoard::finishUpdate() { update(); }
QImage FormationBoard::overviewImage() {
    QImage result(size(), QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    overview_ = true;
    render(&result, {}, {}, QWidget::DrawChildren);
    overview_ = false;
    return result.copy(0, 3 * zoom_, width(), height()-9 * zoom_).scaledToWidth(208, Qt::FastTransformation);
}
void FormationBoard::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    if (!board_.isNull()) p.drawImage(QRectF(origin(), QSizeF(board_.size() * zoom_)), board_);
    else {
        p.setPen(palette().color(QPalette::Text));
        p.drawText(QRect(0, 0, width(), 45), Qt::TextWordWrap, artworkError_);
        for (int i = 0; i < 9; ++i) {
            p.setBrush(i % 2 ? QColor("#283887") : QColor("#c9a149"));
            p.drawPolygon(cellPolygon(i));
        }
    }
    p.setRenderHint(QPainter::Antialiasing);
    for (int i = 0; i < 9; ++i) {
        if (conflicts_[i] || cells_[i]->isChecked() || cells_[i]->hasFocus() || target_ == i) {
            const auto color = conflicts_[i] ? QColor("#ff6040") : QColor("#fff291");
            p.setPen(QPen(color, target_ == i ? 3 : 2));
            p.setBrush(QColor(color.red(), color.green(), color.blue(), 35));
            p.drawPolygon(cellPolygon(i));
        }
        if (!hints_[i].isEmpty()) {
            p.setPen(QPen(QColor("#982018"), 2));
            const auto at = cellCenter(i);
            p.drawLine(at + QPoint(-5,-5), at + QPoint(5,5));
            p.drawLine(at + QPoint(5,-5), at + QPoint(-5,5));
        }
    }
    std::array<int,9> order{0,1,2,3,4,5,6,7,8};
    std::sort(order.begin(), order.end(), [this](int a,int b){return foot(a).y()<foot(b).y();});
    for (int i : order) {
        if (dragging_ && i == pressed_) p.setOpacity(0.25);
        else p.setOpacity(cells_[i]->isEnabled() ? 1 : 0.55);
        if (!sprites_[i].isNull()) p.drawImage(spriteRect(i), sprites_[i]);
        else if (occupied_[i]) {
            p.setPen(palette().color(QPalette::Text));
            p.drawText(QRectF(foot(i)-QPointF(12,30), QSizeF(24,24)), Qt::AlignCenter, "?");
        }
    }
    p.setOpacity(1);
    if (dragging_ && pressed_ >= 0 && !sprites_[pressed_].isNull()) {
        const auto size = sprites_[pressed_].size() * zoom_;
        p.drawImage(QRectF(pointerPosition_ - QPoint(size.width()/2,size.height()/2), size),
                    sprites_[pressed_]);
    }
    p.setPen(palette().color(QPalette::Text));
    if (!overview_)
        p.drawText(QRect(0, height()-23, width(), 23), Qt::AlignCenter,
                   playerFormation_ ? "Front · player squad (cells 7–9)" : "Front · enemy squad (cells 7–9)");
}
void FormationBoard::cancelDrag() {
    pressed_ = target_ = -1; dragging_ = false; update();
}
bool FormationBoard::pointerEvent(QEvent *event, QWidget *source) {
    if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseMove &&
        event->type() != QEvent::MouseButtonRelease) return false;
    const auto mouse = static_cast<QMouseEvent *>(event);
    const auto point = source->mapTo(this, mouse->position().toPoint());
    if (event->type() == QEvent::MouseButtonPress && mouse->button() == Qt::LeftButton) {
        int cell = memberAt(point);
        if (cell < 0) cell = cellAt(point);
        if (cell < 0 || !cells_[cell]->isEnabled()) return true;
        pressed_ = cell; pressPosition_ = point; pointerPosition_ = point;
        cells_[cell]->setFocus(Qt::MouseFocusReason);
        emit cellPressed(cell);
        update();
        return true;
    }
    if (event->type() == QEvent::MouseMove) {
        target_ = cellAt(point);
        if (pressed_ >= 0 && (mouse->buttons() & Qt::LeftButton)) {
            if (!dragging_ && occupied_[pressed_] &&
                (point-pressPosition_).manhattanLength() >= QApplication::startDragDistance()) {
                dragging_ = true;
                emit cellDragStarted(pressed_);
            }
            pointerPosition_ = point;
        }
        update(); return true;
    }
    if (event->type() == QEvent::MouseButtonRelease && mouse->button() == Qt::LeftButton) {
        const int from = pressed_, to = cellAt(point);
        const bool dragged = dragging_;
        cancelDrag();
        if (from >= 0 && dragged && to >= 0 && cells_[to]->isEnabled()) emit cellMoved(from,to);
        else if (from >= 0 && !dragged && cells_[from]->isEnabled()) cells_[from]->click();
        return true;
    }
    return false;
}
bool FormationBoard::eventFilter(QObject *object, QEvent *event) {
    auto widget = qobject_cast<QWidget *>(object);
    if (widget && pointerEvent(event,widget)) return true;
    if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut) update();
    return QWidget::eventFilter(object,event);
}
bool FormationBoard::event(QEvent *event) {
    if (pointerEvent(event,this)) return true;
    if (event->type() == QEvent::Hide || event->type() == QEvent::WindowDeactivate) cancelDrag();
    if (event->type() == QEvent::Leave && pressed_ < 0) {target_=-1;update();}
    if (event->type() == QEvent::ToolTip) {
        auto help = static_cast<QHelpEvent *>(event);
        int cell = memberAt(help->pos());
        if (cell < 0) cell = cellAt(help->pos());
        if (cell >= 0) QToolTip::showText(help->globalPos(), cells_[cell]->toolTip(), this);
        return true;
    }
    return QWidget::event(event);
}
}
