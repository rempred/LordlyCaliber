#include "app/scenario_map_editor.h"
#include "app/scenario_world_editor.h"
#include "app/view_position.h"
#include "engine/battlefield.h"
#include "engine/squad_members.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QImage>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>

namespace lordly {
static QImage mapIcon(const QImage &source, bool portrait) {
    if (source.isNull())
        return {};
    QImage result(32, 32, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath circle;
    circle.addEllipse(QRectF(1, 1, 30, 30));
    painter.setClipPath(circle);
    painter.fillRect(result.rect(), QColor("#1d1a16"));
    if (portrait) {
        const int side = std::min(source.width(), source.height());
        painter.drawImage(
            QRectF(1, 1, 30, 30), source,
            QRectF((source.width() - side) / 2., (source.height() - side) / 2., side, side));
    } else {
        const QSize size = source.size().scaled(24, 24, Qt::KeepAspectRatio);
        painter.drawImage(
            QRectF(QPointF((32 - size.width()) / 2., (32 - size.height()) / 2.), size), source);
    }
    return result;
}
struct MapMarker {
    MapObject kind;
    int row;
    QString label;
    std::optional<QPointF> position;
    QColor color;
    QImage icon;
    bool locked = false;
    qreal radius() const {
        return kind == MapObject::Squad || kind == MapObject::Treasure ? 16.
               : kind == MapObject::Waypoint                           ? 4.
                                                                       : 6.;
    }
    int layer() const {
        return kind == MapObject::Site ? 0 : kind == MapObject::Waypoint ? 1 : 2;
    }
    bool movable() const {
        return !locked;
    }
};
class MapCanvas : public QWidget {
  public:
    explicit MapCanvas(QWidget *parent = nullptr) : QWidget(parent) {
        setObjectName("scenarioMapCanvas");
        setMinimumSize(300, 300);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
    }
    QVector<MapMarker> markers;
    QVector<QPair<int, int>> edges;
    std::function<void(int)> selected;
    std::function<void(int, QPointF)> moved;
    std::function<void(QPointF)> place;
    bool placing = false;
    int brushMode = 0;
    double brushRadius = 3;
    bool straightBrush = false;
    QVector<QPointF> stroke;
    std::function<void(const QVector<QPointF> &)> painted;
    int current = -1;
    bool grid = false, labels = true, showTerrain = true;
    QImage terrain;
    QRectF terrainBounds, boundary;
    bool showBoundary = true, editBoundary = false;
    int boundaryCorner = -1;
    QRectF boundaryDrag;
    std::function<void(QRectF)> resizedBoundary;
    void cancelDrag() {
        drag_ = -1;
        dragging_ = false;
        panning_ = false;
        stroke.clear();
        boundaryCorner = -1;
    }
    void resetView() {
        zoom_ = 1;
        pan_ = {};
        update();
    }
    QPointF toPixel(QPointF p) const {
        const qreal side = std::max(1, std::min(width(), height()) - 72) * zoom_;
        return QPointF(width() / 2., height() / 2.) + pan_ + (p - QPointF(128, 128)) * side / 256.;
    }
    QPointF fromPixel(QPointF p) const {
        const qreal side = std::max(1, std::min(width(), height()) - 72) * zoom_;
        return (p - QPointF(width() / 2., height() / 2.) - pan_) * 256. / side + QPointF(128, 128);
    }

  protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#141e29"));
        const QRectF area(toPixel({0, 0}), toPixel({256, 256}));
        p.fillRect(area, QColor("#1c2c39"));
        if (showTerrain && !terrain.isNull()) {
            p.save();
            p.setClipRect(area);
            p.drawImage(
                QRectF(toPixel(terrainBounds.topLeft()), toPixel(terrainBounds.bottomRight())),
                terrain);
            p.restore();
        }
        p.setPen(QColor("#526678"));
        p.drawRect(area);
        if (showBoundary && boundary.isValid()) {
            const auto edge = boundaryCorner >= 0 ? boundaryDrag : boundary;
            const QRectF outline(toPixel(edge.topLeft()), toPixel(edge.bottomRight()));
            QPainterPath outside;
            outside.addRect(area);
            outside.addRect(outline);
            p.fillPath(outside, QColor(8, 14, 20, 32));
            p.setPen(QPen(QColor("#ffcf67"), editBoundary ? 2.5 : 1.5, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(outline);
            p.drawText(outline.topLeft() + QPointF(6, 16), "Playable boundary");
            if (editBoundary) {
                p.setBrush(QColor("#fff2a8"));
                for (auto point : {outline.topLeft(), outline.topRight(), outline.bottomRight(),
                                   outline.bottomLeft()})
                    p.drawRect(QRectF(point - QPointF(5, 5), QSizeF(10, 10)));
            }
        }
        if (grid)
            for (int n = 0; n <= 256; n += 32) {
                p.setPen(QColor("#344856"));
                p.drawLine(toPixel({qreal(n), 0}), toPixel({qreal(n), 256}));
                p.drawLine(toPixel({0, qreal(n)}), toPixel({256, qreal(n)}));
                p.setPen(QColor("#a6bdc9"));
                p.drawText(toPixel({qreal(n), 0}) + QPointF(-8, -10), QString::number(n));
                p.drawText(toPixel({0, qreal(n)}) + QPointF(-32, 4), QString::number(n));
            }
        p.setPen(QPen(QColor("#678b98"), 1.5, Qt::DashLine));
        for (const auto &[a, b] : edges)
            if (markers[a].position && markers[b].position)
                p.drawLine(toPixel(*markers[a].position), toPixel(*markers[b].position));
        QVector<QRectF> labelAreas;
        QVector<int> drawOrder;
        if (current >= 0 && current < markers.size())
            drawOrder << current;
        for (int i = 0; i < markers.size(); ++i)
            if (i != current)
                drawOrder << i;
        // Draw the selected marker last, then reserve icon bounds while placing labels.
        auto paintOrder = drawOrder;
        std::stable_sort(paintOrder.begin(), paintOrder.end(), [&](int a, int b) {
            return (a == current ? 3 : markers[a].layer()) <
                   (b == current ? 3 : markers[b].layer());
        });
        for (int i : paintOrder) {
            const auto &m = markers[i];
            if (!m.position)
                continue;
            const QPointF center =
                i == drag_ && dragging_ ? mouse_ - dragOffset_ : toPixel(*m.position);
            const qreal radius = m.radius();
            labelAreas << QRectF(center - QPointF(radius + 3, radius + 3),
                                 QSizeF((radius + 3) * 2, (radius + 3) * 2));
            p.setPen(
                QPen(i == current ? QColor("#fff2a8") : QColor("#0d131a"), i == current ? 3 : 1.5));
            p.setBrush(m.color);
            if (!m.icon.isNull()) {
                p.drawImage(QRectF(center - QPointF(16, 16), QSizeF(32, 32)), m.icon);
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(m.color, 2));
                p.drawEllipse(center, 15., 15.);
                if (i == current) {
                    p.setPen(QPen(QColor("#fff2a8"), 3));
                    p.drawEllipse(center, 18., 18.);
                }
            } else if (m.kind == MapObject::Site) {
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(i == current ? QColor("#fff2a8") : m.color, i == current ? 3 : 1));
                p.drawRect(QRectF(center - QPointF(6, 6), QSizeF(12, 12)));
            } else if (m.kind == MapObject::Treasure) {
                QPolygonF diamond;
                diamond << center + QPointF(0, -7) << center + QPointF(7, 0)
                        << center + QPointF(0, 7) << center + QPointF(-7, 0);
                p.drawPolygon(diamond);
            } else
                p.drawEllipse(center, radius, radius);
        }
        for (int i : drawOrder) {
            const auto &m = markers[i];
            if (!m.position)
                continue;
            const QPointF center =
                i == drag_ && dragging_ ? mouse_ - dragOffset_ : toPixel(*m.position);
            if (labels || i == current) {
                const QString text = m.label.section(" · ", 0, 0);
                QRectF label(center + QPointF(m.radius() + 6, -10),
                             QSizeF(p.fontMetrics().horizontalAdvance(text) + 8, 20));
                for (int attempt = 0; attempt < 12; ++attempt) {
                    if (std::none_of(labelAreas.begin(), labelAreas.end(),
                                     [&](const QRectF &other) { return other.intersects(label); }))
                        break;
                    label.translate(0, 21);
                }
                labelAreas << label;
                p.fillRect(label, QColor(12, 20, 28, 210));
                p.setPen(Qt::white);
                p.drawText(label.adjusted(4, 0, 0, 0), Qt::AlignVCenter, text);
            }
        }
        if (brushMode && underMouse()) {
            const auto radius = QLineF(toPixel({0, 0}), toPixel({qreal(brushRadius), 0})).length();
            p.save();
            p.setClipRect(area);
            p.setPen(QPen(QColor(250, 220, 130, 150), radius * 2, Qt::SolidLine,
                          straightBrush ? Qt::SquareCap : Qt::RoundCap, Qt::RoundJoin));
            for (int i = 1; i < stroke.size(); ++i)
                p.drawLine(toPixel(stroke[i - 1]), toPixel(stroke[i]));
            p.setPen(QPen(QColor("#fff2a8"), 1.5));
            p.setBrush(QColor(250, 220, 130, 50));
            if (!straightBrush)
                p.drawEllipse(mouse_, radius, radius);
            else if (stroke.size() < 2)
                p.drawRect(
                    QRectF(mouse_ - QPointF(radius, radius), QSizeF(radius * 2, radius * 2)));
            p.restore();
        }
        p.setPen(QColor("#bdd0dc"));
        p.drawText(QRect(10, height() - 25, width() - 20, 20),
                   "X →   Z ↓     Wheel: zoom · Right-drag: pan");
    }
    void mousePressEvent(QMouseEvent *e) override {
        press_ = mouse_ = e->position();
        if (e->button() == Qt::RightButton) {
            panning_ = true;
            return;
        }
        if (e->button() != Qt::LeftButton)
            return;
        if (placing && place) {
            place(fromPixel(e->position()));
            return;
        }
        if (editBoundary) {
            const QPointF corners[] = {boundary.topLeft(), boundary.topRight(),
                                       boundary.bottomRight(), boundary.bottomLeft()};
            for (int i = 0; i < 4; ++i)
                if (QLineF(toPixel(corners[i]), press_).length() < 14) {
                    boundaryCorner = i;
                    boundaryDrag = boundary;
                    break;
                }
            update();
            return;
        }
        if (brushMode) {
            stroke = {fromPixel(e->position())};
            update();
            return;
        }
        int hit = -1;
        qreal distance = std::numeric_limits<qreal>::max();
        // Keep a list-selected marker reachable when several share a position.
        if (current >= 0 && current < markers.size() && markers[current].position &&
            QLineF(toPixel(*markers[current].position), press_).length() <=
                std::max(12., markers[current].radius() + 2))
            hit = current;
        else
            for (int i = 0; i < markers.size(); ++i)
                if (markers[i].position) {
                    const auto d = QLineF(toPixel(*markers[i].position), press_).length();
                    if (d <= std::max(12., markers[i].radius() + 2) &&
                        (hit < 0 || markers[i].layer() > markers[hit].layer() ||
                         (markers[i].layer() == markers[hit].layer() && d <= distance))) {
                        hit = i;
                        distance = d;
                    }
                }
        current = hit;
        if (selected)
            selected(hit);
        drag_ = hit >= 0 && markers[hit].movable() ? hit : -1;
        dragOffset_ = drag_ >= 0 ? press_ - toPixel(*markers[drag_].position) : QPointF();
        update();
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        if (panning_)
            pan_ += e->position() - mouse_;
        mouse_ = e->position();
        if (boundaryCorner >= 0) {
            const auto p = fromPixel(mouse_);
            if (boundaryCorner == 0 || boundaryCorner == 3)
                boundaryDrag.setLeft(std::clamp(p.x(), 1., boundaryDrag.right() - 8));
            else
                boundaryDrag.setRight(std::clamp(p.x(), boundaryDrag.left() + 8, 254.));
            if (boundaryCorner < 2)
                boundaryDrag.setTop(std::clamp(p.y(), 1., boundaryDrag.bottom() - 8));
            else
                boundaryDrag.setBottom(std::clamp(p.y(), boundaryDrag.top() + 8, 254.));
        }
        if (!stroke.isEmpty() && !panning_)
            extendStroke(fromPixel(mouse_));
        if (drag_ >= 0 && QLineF(press_, mouse_).length() > 3)
            dragging_ = true;
        update();
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        if (e->button() == Qt::RightButton) {
            panning_ = false;
            return;
        }
        if (e->button() != Qt::LeftButton)
            return;
        if (boundaryCorner >= 0) {
            boundaryCorner = -1;
            if (resizedBoundary)
                resizedBoundary(boundaryDrag);
            update();
            return;
        }
        if (!stroke.isEmpty()) {
            extendStroke(fromPixel(e->position()));
            const auto completed = stroke;
            stroke.clear();
            if (painted)
                painted(completed);
            update();
            return;
        }
        const int index = drag_;
        const bool changed = dragging_;
        drag_ = -1;
        dragging_ = false;
        if (changed && moved)
            moved(index, fromPixel(e->position() - dragOffset_));
        update();
    }
    void wheelEvent(QWheelEvent *e) override {
        const auto before = fromPixel(e->position());
        zoom_ = std::clamp(zoom_ * std::pow(1.0015, e->angleDelta().y()), .5, 8.);
        pan_ += e->position() - toPixel(before);
        update();
        e->accept();
    }

  private:
    void extendStroke(QPointF p) {
        if (straightBrush && stroke.size() > 1)
            stroke.last() = p;
        else
            stroke << p;
    }
    qreal zoom_ = 1;
    QPointF pan_, mouse_, press_, dragOffset_;
    int drag_ = -1;
    bool dragging_ = false, panning_ = false;
};
class MapWorldCommand final : public QUndoCommand {
  public:
    MapWorldCommand(Project *p, QJsonObject before, QJsonObject after,
                    const QString &label = "Move treasure on map")
        : QUndoCommand(label), project_(p), before_(std::move(before)), after_(std::move(after)) {}
    void undo() override {
        project_->setWorld(before_);
    }
    void redo() override {
        project_->setWorld(after_);
    }

  private:
    Project *project_;
    QJsonObject before_, after_;
};
class FieldEditCommand final : public QUndoCommand {
  public:
    FieldEditCommand(Project *p, QJsonObject before, QJsonObject after, const QString &label)
        : QUndoCommand(label), project_(p), before_(std::move(before)), after_(std::move(after)) {}
    void undo() override {
        project_->setMapEdits(before_);
    }
    void redo() override {
        project_->setMapEdits(after_);
    }

  private:
    Project *project_;
    QJsonObject before_, after_;
};
ScenarioMapEditor::ScenarioMapEditor(Project *project, QUndoStack *undo,
                                     std::function<void(const Scenarios &, const QString &)> commit,
                                     std::function<void(MapObject, int)> editFields,
                                     QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), commit_(std::move(commit)),
      editFields_(std::move(editFields)) {
    auto layout = new QVBoxLayout(this);
    auto toolbar = new QHBoxLayout;
    auto fit = new QPushButton("Fit map");
    fit->setObjectName("mapFit");
    toolbar->addWidget(fit);
    auto addTreasure = new QPushButton("Add treasure on map");
    addTreasure->setObjectName("mapTreasureAdd");
    addTreasure->setCheckable(true);
    toolbar->addWidget(addTreasure);
    auto grid = new QCheckBox("Grid"), labels = new QCheckBox("Labels");
    grid->setChecked(false);
    labels->setChecked(true);
    toolbar->addWidget(grid);
    toolbar->addWidget(labels);
    terrain_ = new QCheckBox("Battlefield");
    terrain_->setObjectName("mapTerrain");
    terrain_->setToolTip("Show the in-scenario terrain, roads and buildings from your ROM.");
    terrain_->setChecked(true);
    toolbar->addWidget(terrain_);
    toolbar->addStretch();
    layout->addLayout(toolbar);
    paintTools_ = new QWidget;
    auto painting = new QHBoxLayout(paintTools_);
    painting->setContentsMargins(0, 0, 0, 0);
    tool_ = new QComboBox;
    tool_->setObjectName("mapTool");
    tool_->addItems(
        {"Select / move", "Paint terrain", "Draw road", "Erase painting", "Edit boundary"});
    painting->addWidget(tool_);
    paintType_ = new QComboBox;
    paintType_->setObjectName("mapPaintType");
    for (int type = 1; type <= 14; ++type)
        paintType_->addItem(Encounters::mapTerrainName(type), type);
    paintType_->setCurrentIndex(2);
    paintType_->setToolTip("Drag from one bridge end to the other for a straight span. Leave a gap "
                           "between separate bridges.");
    painting->addWidget(paintType_);
    painting->addWidget(new QLabel("Brush radius"));
    brushSize_ = new QDoubleSpinBox;
    brushSize_->setObjectName("mapBrushSize");
    brushSize_->setRange(1, 16);
    brushSize_->setDecimals(1);
    brushSize_->setSingleStep(.5);
    brushSize_->setValue(3);
    painting->addWidget(brushSize_);
    auto restore = new QPushButton("Restore map edits");
    restore->setObjectName("mapRestore");
    painting->addWidget(restore);
    painting->addStretch();
    layout->addWidget(paintTools_);
    auto boundaryToggle = new QCheckBox("Boundary");
    boundaryToggle->setObjectName("mapBoundary");
    boundaryToggle->setChecked(true);
    toolbar->addWidget(boundaryToggle);
    boundaryTools_ = new QWidget;
    auto boundaryLayout = new QHBoxLayout(boundaryTools_);
    boundaryLayout->setContentsMargins(0, 0, 0, 0);
    const QStringList edgeNames{"Left", "Top", "Right", "Bottom"};
    for (int i = 0; i < 4; ++i) {
        boundaryLayout->addWidget(new QLabel(edgeNames[i]));
        auto box = new QSpinBox;
        box->setRange(1, 254);
        box->setObjectName("mapBoundary" + edgeNames[i]);
        boundaryFields_[i] = box;
        boundaryLayout->addWidget(box);
    }
    auto applyBoundary = new QPushButton("Apply boundary");
    applyBoundary->setObjectName("mapBoundaryApply");
    boundaryLayout->addWidget(applyBoundary);
    boundaryLayout->addStretch();
    layout->addWidget(boundaryTools_);
    boundaryTools_->hide();
    mapScope_ = new QLabel;
    mapScope_->setObjectName("mapEditScope");
    mapScope_->setWordWrap(true);
    layout->addWidget(mapScope_);
    paintType_->setToolTip("Paint over existing roads with Plain or another terrain to remove "
                           "them. Highlands raise the ground; forests add a canopy and edge. "
                           "Existing landforms and fixed fortress walls remain.");
    tool_->setToolTip("Drag to paint. Erase painting restores the loaded ROM in that area. Each "
                      "stroke can be undone.");
    auto split = new QSplitter;
    canvas_ = new MapCanvas;
    split->addWidget(canvas_);
    auto sidebar = new QWidget;
    auto side = new QVBoxLayout(sidebar);
    objects_ = new QListWidget;
    objects_->setObjectName("mapObjects");
    objects_->setIconSize(QSize(32, 32));
    objects_->setMinimumHeight(90);
    objects_->setMaximumHeight(150);
    side->addWidget(objects_);
    auto inspector = new QWidget;
    auto fields = new QVBoxLayout(inspector);
    fields->setContentsMargins(0, 0, 0, 0);
    fields->setSizeConstraint(QLayout::SetMinimumSize);
    auto scroll = new QScrollArea;
    scroll->setObjectName("mapInspectorScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(inspector);
    side->addWidget(scroll, 1);
    selection_ = new QLabel;
    selection_->setObjectName("mapSelectionTitle");
    selection_->setTextFormat(Qt::PlainText);
    selection_->setWordWrap(true);
    auto headingFont = selection_->font();
    headingFont.setBold(true);
    headingFont.setPointSize(12);
    selection_->setFont(headingFont);
    fields->addWidget(selection_);
    summary_ = new QLabel;
    summary_->setObjectName("mapSelectionSummary");
    summary_->setTextFormat(Qt::PlainText);
    summary_->setWordWrap(true);
    fields->addWidget(summary_);
    coordinates_ = new QWidget;
    auto coords = new QHBoxLayout(coordinates_);
    coords->setContentsMargins(0, 0, 0, 0);
    x_ = new QSpinBox;
    z_ = new QSpinBox;
    x_->setRange(0, 255);
    z_->setRange(0, 255);
    x_->setObjectName("mapX");
    z_->setObjectName("mapZ");
    coords->addWidget(new QLabel("X"));
    coords->addWidget(x_);
    coords->addWidget(new QLabel("Z"));
    coords->addWidget(z_);
    fields->addWidget(coordinates_);
    apply_ = new QPushButton("Set map coordinates");
    apply_->setObjectName("mapApply");
    fields->addWidget(apply_);
    fields_ = new QPushButton("Edit selected fields");
    fields_->setObjectName("mapEditFields");
    fields->addWidget(fields_);
    // The shared object list owns selection; site/treasure forms need no second picker.
    world_ = new ScenarioWorldEditor(project_, undo_, [] {});
    fields->addWidget(world_);
    world_->hide();
    referenceName_ = new QLabel;
    referenceName_->setWordWrap(true);
    referenceName_->setObjectName("mapTerrainStatus");
    fields->addWidget(referenceName_);
    reference_ = new QLabel;
    reference_->setAlignment(Qt::AlignCenter);
    fields->addWidget(reference_);
    fields->addStretch();
    sidebar->setMinimumWidth(330);
    split->addWidget(sidebar);
    split->setSizes({700, 370});
    layout->addWidget(split, 1);
    notice_ = new QLabel;
    notice_->setObjectName("mapNotice");
    notice_->setWordWrap(true);
    layout->addWidget(notice_);
    canvas_->resizedBoundary = [this](QRectF b) { changeBoundary(b); };
    connect(applyBoundary, &QPushButton::clicked, this, [this] {
        changeBoundary(QRectF(QPointF(boundaryFields_[0]->value(), boundaryFields_[1]->value()),
                              QPointF(boundaryFields_[2]->value(), boundaryFields_[3]->value())));
    });
    connect(boundaryToggle, &QCheckBox::toggled, this, [this](bool value) {
        canvas_->showBoundary = value;
        canvas_->update();
    });
    canvas_->painted = [this](const QVector<QPointF> &stroke) { paint(stroke); };
    connect(tool_, &QComboBox::currentIndexChanged, this,
            [this, addTreasure, boundaryToggle](int mode) {
                canvas_->cancelDrag();
                canvas_->brushMode = mode < 4 ? mode : 0;
                const int terrain = paintType_->currentData().toInt();
                canvas_->straightBrush = mode == 1 && (terrain == 2 || terrain == 10);
                canvas_->editBoundary = mode == 4 && key_ != 39;
                boundaryTools_->setVisible(mode == 4);
                boundaryTools_->setEnabled(key_ != 39);
                if (mode == 4)
                    boundaryToggle->setChecked(true);
                if (mode)
                    addTreasure->setChecked(false);
                paintType_->setEnabled(mode == 1);
                brushSize_->setEnabled(mode > 0 && mode < 4);
                canvas_->setCursor(mode ? Qt::CrossCursor : Qt::ArrowCursor);
                canvas_->update();
            });
    paintType_->setEnabled(false);
    connect(paintType_, &QComboBox::currentIndexChanged, this, [this] {
        canvas_->cancelDrag();
        const int terrain = paintType_->currentData().toInt();
        canvas_->straightBrush = tool_->currentIndex() == 1 && (terrain == 2 || terrain == 10);
        canvas_->update();
    });
    brushSize_->setEnabled(false);
    connect(brushSize_, &QDoubleSpinBox::valueChanged, this, [this](double n) {
        canvas_->brushRadius = n;
        canvas_->update();
    });
    connect(restore, &QPushButton::clicked, this, [this] {
        try {
            auto next = *project_->mapEdits();
            const auto before = next.json();
            next.restore(key_);
            if (next.json() != before) {
                undo_->push(new FieldEditCommand(project_, before, next.json(),
                                                 "Restore map terrain and buildings"));
                refresh(key_);
            }
        } catch (const Error &e) {
            notice_->setText(QString::fromUtf8(e.what()));
        }
    });
    canvas_->selected = [this](int row) { objects_->setCurrentRow(row); };
    canvas_->moved = [this](int row, QPointF position) {
        if (row < 0 || row >= canvas_->markers.size() || !maps_.contains(key_))
            return;
        const auto &m = maps_[key_];
        const QPointF world(m.bounds.left() + m.bounds.width() * position.x() / 256.,
                            m.bounds.top() + m.bounds.height() * position.y() / 256.);
        const auto kind = canvas_->markers[row].kind;
        move(row, m.bytes(world, kind == MapObject::Squad || kind == MapObject::Waypoint));
    };
    connect(addTreasure, &QPushButton::toggled, this, [this](bool on) {
        if (on)
            tool_->setCurrentIndex(0);
        canvas_->placing = on;
        canvas_->setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
        notice_->setText(on ? "Click the map to place a new treasure. Click Add treasure again to "
                              "cancel. Choose its item in the sidebar."
                            : "");
    });
    canvas_->place = [this, addTreasure](QPointF position) {
        if (key_ < 1 || !maps_.contains(key_))
            return;
        if (position.x() < 0 || position.x() > 255 || position.y() < 0 || position.y() > 255) {
            notice_->setText("Place the treasure inside the map bounds.");
            return;
        }
        try {
            auto next = *project_->world();
            const int row = next.addTreasure(key_);
            auto record = ScenarioWorld::treasureRecord(next.scenario(key_).treasures, row);
            put8(record, 1, quint8(std::round(position.x())));
            put8(record, 2, quint8(std::round(position.y())));
            next.setTreasure(key_, row, record);
            addTreasure->setChecked(false);
            undo_->push(new MapWorldCommand(project_, project_->world()->json(), next.json(),
                                            "Place new treasure on map"));
            refresh(key_);
            for (int i = 0; i < canvas_->markers.size(); ++i)
                if (canvas_->markers[i].kind == MapObject::Treasure &&
                    canvas_->markers[i].row == row) {
                    objects_->setCurrentRow(i);
                    break;
                }
            notice_->setText("Treasure placed. Choose its item in the sidebar, or "
                             "drag to move it.");
        } catch (const Error &e) {
            notice_->setText(QString::fromUtf8(e.what()));
        }
    };
    connect(fit, &QPushButton::clicked, this, [this] { canvas_->resetView(); });
    connect(terrain_, &QCheckBox::toggled, this, [this](bool value) {
        canvas_->showTerrain = value;
        canvas_->update();
    });
    connect(grid, &QCheckBox::toggled, this, [this](bool value) {
        canvas_->grid = value;
        canvas_->update();
    });
    connect(labels, &QCheckBox::toggled, this, [this](bool value) {
        canvas_->labels = value;
        canvas_->update();
    });
    connect(objects_, &QListWidget::currentRowChanged, this, [this](int row) { select(row); });
    connect(apply_, &QPushButton::clicked, this,
            [this] { move(objects_->currentRow(), {x_->value(), z_->value()}); });
    connect(fields_, &QPushButton::clicked, this, [this] {
        const int row = objects_->currentRow();
        if (row >= 0) {
            const auto marker = canvas_->markers[row];
            editFields_(marker.kind, marker.row);
        }
    });
}
void ScenarioMapEditor::refresh(int key) {
    const ViewPosition keepView(this, key_ == key);
    world_->refresh(key);
    if (key < 1) {
        paintTools_->setEnabled(false);
        tool_->setCurrentIndex(0);
        canvas_->cancelDrag();
        mapScope_->setText("Choose a scenario to edit its map.");
        key_ = key;
        objects_->clear();
        canvas_->markers.clear();
        canvas_->edges.clear();
        select(-1);
        return;
    }
    const int previous = key_ == key ? objects_->currentRow() : -1;
    if (key_ != key) {
        canvas_->resetView();
        findChild<QPushButton *>("mapTreasureAdd")->setChecked(false);
    }
    key_ = key;
    canvas_->cancelDrag();
    loading_ = true;
    QSignalBlocker block(objects_);
    objects_->clear();
    canvas_->markers.clear();
    canvas_->edges.clear();
    canvas_->current = -1;
    reference_->clear();
    canvas_->terrain = {};
    canvas_->terrainBounds = {};
    canvas_->boundary = {};
    canvas_->editBoundary = false;
    boundaryTools_->setEnabled(false);
    terrain_->setEnabled(false);
    paintTools_->setEnabled(false);
    referenceName_->clear();
    notice_->clear();
    try {
        QStringList artworkWarnings;
        try {
            project_->loadAvatarArtwork();
        } catch (const Error &e) {
            artworkWarnings << QString::fromUtf8(e.what());
        }
        try {
            project_->loadIconArtwork();
        } catch (const Error &e) {
            artworkWarnings << QString::fromUtf8(e.what());
        }
        QMap<int, QImage> portraits, itemIcons;
        const auto classes = project_->classData();
        auto artwork = [&](int id, bool portrait, int selector = 0) -> QImage {
            auto &cache = portrait ? portraits : itemIcons;
            const int cacheKey = portrait ? id * 2 + selector : id;
            if (!cache.contains(cacheKey)) {
                QImage image;
                try {
                    if (portrait && project_->avatarArtwork()) {
                        auto words = project_->avatarArtwork()->words(classes, id, selector);
                        if (words.isEmpty())
                            words = project_->avatarArtwork()->words(classes, id, 1 - selector);
                        if (!words.isEmpty())
                            image = AvatarArtwork::render(words);
                    }
                    if (!portrait && project_->iconArtwork())
                        image = project_->iconArtwork()->image(quint16(id));
                } catch (const Error &e) {
                    artworkWarnings << QString::fromUtf8(e.what());
                }
                cache[cacheKey] = mapIcon(image, portrait);
            }
            return cache[cacheKey];
        };
        if (!maps_.contains(key))
            maps_[key] = ScenarioMap::load(project_->rom(), key);
        auto map = maps_[key];
        bool editable = false;
        try {
            project_->loadMapEdits();
            map.points = project_->mapEdits()->points(key);
            editable = true;
            canvas_->boundary = project_->mapEdits()->boundary(key);
            canvas_->editBoundary = tool_->currentIndex() == 4 && key != 39;
            boundaryTools_->setEnabled(key != 39);
            boundaryTools_->setToolTip(key == 39
                                           ? "Fort Romulus uses a separate scripted boundary; "
                                             "resizing is not supported yet."
                                           : "Drag the boundary corners, or enter its edges here.");
            const auto edge = canvas_->boundary;
            const double values[] = {edge.left(), edge.top(), edge.right(), edge.bottom()};
            for (int i = 0; i < 4; ++i)
                boundaryFields_[i]->setValue(int(values[i]));
            QStringList keys;
            for (int k : project_->mapEdits()->sharedScenarios(key))
                keys << QString::number(k);
            mapScope_->setText("Map edits affect runtime keys " + keys.join(", ") +
                               ". Highlands raise the ground. Export also updates the briefing "
                               "image and minimap.");
            if (key == 39) {
                canvas_->boundary = {};
                mapScope_->setText(mapScope_->text() +
                                   " Fort Romulus has a scripted boundary; its outline and "
                                   "resizing are not supported yet.");
            }
        } catch (const Error &e) {
            mapScope_->setText(QString::fromUtf8(e.what()));
        }
        paintTools_->setEnabled(editable);
        if (!editable)
            tool_->setCurrentIndex(0);
        auto normalize = [&](std::optional<QPointF> point) -> std::optional<QPointF> {
            if (!point)
                return {};
            return QPointF((point->x() - map.bounds.left()) / map.bounds.width() * 256.,
                           (point->y() - map.bounds.top()) / map.bounds.height() * 256.);
        };
        auto add = [&](MapObject kind, int row, QString label, std::optional<QPointF> position,
                       QColor color, QImage icon = {}) {
            const bool locked = kind == MapObject::Site &&
                                (!editable || !project_->mapEdits()->canMoveSite(key, row));
            canvas_->markers << MapMarker{kind,  row,  label, normalize(position),
                                          color, icon, locked};
            auto item =
                new QListWidgetItem(label + (position ? "" : " · unresolved placement"), objects_);
            item->setData(Qt::UserRole, int(kind));
            item->setData(Qt::UserRole + 1, row);
            item->setForeground(color.darker(140));
            if (!icon.isNull())
                item->setIcon(QPixmap::fromImage(icon));
            item->setToolTip(item->text());
        };
        const auto &s = project_->scenarios()->scenario(key);
        for (int i = 0; i < s.layout.deployments.size(); ++i) {
            const auto &d = s.layout.deployments[i];
            const int id = u8(s.squads[i], 0);
            add(MapObject::Squad, i,
                QString("Squad %1 · %2")
                    .arg(u8(d, 0) & 127)
                    .arg(QString(project_->record(Table::Classes, id).name).replace('\n', ' ')),
                map.placement(u8(d, 3), u8(d, 4)),
                u8(d, 0) & 128 ? QColor("#69c9f0") : QColor("#f08780"),
                artwork(id, true, u8(d, 0) & 128 ? 0 : 1));
        }
        QMap<int, int> nodeMarkers;
        for (int i = 0; i < s.layout.nodes.size(); ++i) {
            const auto &n = s.layout.nodes[i];
            if (u8(n, 1) != 1)
                continue;
            nodeMarkers[u8(n, 0)] = int(canvas_->markers.size());
            add(MapObject::Waypoint, i, QString("Waypoint %1").arg(u8(n, 0)),
                map.placement(u8(n, 4), u8(n, 5)), QColor("#99dfb0"));
        }
        for (auto it = nodeMarkers.begin(); it != nodeMarkers.end(); ++it) {
            const auto &n = s.layout.nodes[canvas_->markers[it.value()].row];
            for (int at : {16, 17})
                if (nodeMarkers.contains(u8(n, at)))
                    canvas_->edges << qMakePair(it.value(), nodeMarkers[u8(n, at)]);
        }
        for (int i = 0; i < s.layout.deployments.size(); ++i)
            if (nodeMarkers.contains(u8(s.layout.deployments[i], 6)))
                canvas_->edges << qMakePair(i, nodeMarkers[u8(s.layout.deployments[i], 6)]);
        QVector<quint16> siteFlags;
        if (project_->world()) {
            const auto &world = *project_->world();
            const auto &scene = world.scenario(key);
            for (int i = 0; i < world.siteCount(key); ++i) {
                const int id = u16(scene.sites, 1 + i * 4);
                const auto town = world.town(id);
                siteFlags << quint16(u16(town, 25) + u16(scene.sites, 3 + i * 4));
                add(MapObject::Site, i, world.townName(id), map.placement(i + 1, 0),
                    QColor("#c9b4eb"));
            }
            for (int i = 0; i < ScenarioWorld::treasureCount(scene.treasures); ++i) {
                const auto r = ScenarioWorld::treasureRecord(scene.treasures, i);
                const int itemId = u16(r, 4);
                const bool equipment = u8(r, 3) == 1;
                const auto &records =
                    project_->records(equipment ? Table::Items : Table::Consumables);
                const QString name =
                    itemId < records.size() ? records[itemId].name : QString("Item %1").arg(itemId);
                add(MapObject::Treasure, i,
                    QString("Treasure %1 · %2 · flag %3").arg(i + 1).arg(name).arg(u8(r, 0)),
                    map.world(u8(r, 1), u8(r, 2)), QColor("#f4cc6c"),
                    artwork(itemId | (equipment ? 0 : 0x200), false));
            }
        }
        if (map.image) {
            const auto &r = *map.image;
            QImage image(reinterpret_cast<const uchar *>(r.rgba.constData()), r.width, r.height,
                         r.width * 4, QImage::Format_RGBA8888);
            reference_->setPixmap(QPixmap::fromImage(image).scaled(260, 170, Qt::KeepAspectRatio,
                                                                   Qt::FastTransformation));
        }
        QString cacheKey = QString::number(key);
        if (editable)
            cacheKey +=
                ':' +
                QString::fromLatin1(
                    QCryptographicHash::hash(
                        QJsonDocument(project_->mapEdits()->json()).toJson(QJsonDocument::Compact),
                        QCryptographicHash::Sha256)
                        .toHex());
        for (auto flag : siteFlags)
            cacheKey += ':' + QString::number(flag);
        try {
            if (!battlefieldImages_.contains(cacheKey)) {
                const auto field =
                    Battlefield::load(project_->rom(), key, siteFlags,
                                      editable ? project_->mapEdits()->model(key) : QByteArray());
                auto image = new QImage(field.render(map.bounds));
                battlefieldNotices_[cacheKey] = field.notices();
                battlefieldImages_.insert(cacheKey, image, int(image->sizeInBytes() / 1024));
            }
            canvas_->terrain = *battlefieldImages_.object(cacheKey);
            canvas_->terrainBounds = QRectF(0, 0, 256, 256);
            terrain_->setEnabled(true);
            if (editable) {
                const int size = project_->mapEdits()->model(key).size();
                mapScope_->setText(
                    mapScope_->text() +
                    QString("  Map detail: %1% used.").arg((size * 100 + 524287) / 524288));
            }
            referenceName_->setText("In-scenario battlefield · Terrain, roads and buildings\n"
                                    "Top-down view with approximate daylight lighting. "
                                    "Export refreshes the briefing image and minimap.");
            if (!battlefieldNotices_[cacheKey].isEmpty())
                referenceName_->setText(referenceName_->text() + '\n' +
                                        battlefieldNotices_[cacheKey].join(' '));
        } catch (const Error &e) {
            referenceName_->setText(
                "Battlefield preview unavailable: " + QString::fromUtf8(e.what()) +
                (map.image ? "\nSeparate briefing overview shown below." : ""));
        }
        notice_->setText("Drag towns, strongholds, squads, waypoints or treasure to set "
                         "positions. Selecting a point-based squad or waypoint "
                         "and moving it changes its placement to map coordinates. " +
                         map.placementNotice);
        artworkWarnings.removeDuplicates();
        if (!artworkWarnings.isEmpty())
            notice_->setText(notice_->text() + "\nSome marker artwork could not be decoded: " +
                             artworkWarnings.join("; "));
        objects_->setCurrentRow(canvas_->markers.isEmpty()
                                    ? -1
                                    : std::clamp(previous, 0, int(canvas_->markers.size()) - 1));
    } catch (const Error &e) {
        notice_->setText(QString::fromUtf8(e.what()));
    }
    loading_ = false;
    select(objects_->currentRow());
    canvas_->update();
}
void ScenarioMapEditor::select(int row) {
    if (loading_)
        return;
    canvas_->current = row;
    canvas_->update();
    const bool valid = row >= 0 && row < canvas_->markers.size();
    const bool movable = valid && canvas_->markers[row].movable();
    const bool worldObject = valid && (canvas_->markers[row].kind == MapObject::Site ||
                                       canvas_->markers[row].kind == MapObject::Treasure);
    world_->setVisible(worldObject);
    coordinates_->setVisible(
        valid && (!worldObject || (canvas_->markers[row].kind == MapObject::Site && movable)));
    apply_->setVisible(
        valid && (!worldObject || (canvas_->markers[row].kind == MapObject::Site && movable)));
    fields_->setVisible(valid && !worldObject);
    summary_->setVisible(valid && (!worldObject || canvas_->markers[row].kind == MapObject::Site));
    reference_->setVisible(!worldObject && canvas_->terrain.isNull());
    referenceName_->setVisible(!worldObject);
    x_->setEnabled(movable);
    z_->setEnabled(movable);
    apply_->setEnabled(movable);
    fields_->setEnabled(valid);
    selection_->clear();
    summary_->clear();
    if (!valid) {
        selection_->setText("Select a squad, waypoint, site or treasure on the map.");
        return;
    }
    const auto &m = canvas_->markers[row];
    selection_->setText(m.label.section(" · flag ", 0, 0));
    if (worldObject) {
        world_->selectObject(m.kind == MapObject::Treasure, m.row);
        if (m.kind == MapObject::Site)
            summary_->setText(movable ? "Drag this building on the map to move its appearance and "
                                        "gameplay location together."
                                      : "This site has no separate building group yet. "
                                        "Its position is not editable.");
    } else if (m.kind == MapObject::Squad) {
        const auto &scenario = project_->scenarios()->scenario(key_);
        const auto members = squadMembers(scenario.squads[m.row]);
        const auto &deployment = scenario.layout.deployments[m.row];
        QStringList lines{u8(deployment, 0) & 128 ? "Allied squad" : "Enemy squad"};
        for (const auto &member : members)
            lines << QString("%1 · Level %2")
                         .arg(QString(project_->record(Table::Classes, member.classId).name)
                                  .replace('\n', ' '))
                         .arg(scenario.layout.baseLevel() + member.levelOffset);
        summary_->setText(lines.join('\n'));
        fields_->setText("Edit squad members and deployment…");
    } else {
        summary_->setText("Route waypoint · connected paths are shown on the map.");
        fields_->setText("Edit route and conditions…");
    }
    z_->setMinimum(m.kind == MapObject::Treasure || m.kind == MapObject::Site ? 0 : 1);
    if (m.position) {
        x_->setValue(int(std::round(m.position->x())));
        z_->setValue(int(std::round(m.position->y())));
    } else {
        x_->setValue(128);
        z_->setValue(128);
    }
}
void ScenarioMapEditor::move(int row, QPoint bytes) {
    if (row < 0 || row >= canvas_->markers.size())
        return;
    const auto marker = canvas_->markers[row];
    if (!marker.movable())
        return;
    try {
        if (marker.kind == MapObject::Site) {
            auto next = *project_->mapEdits();
            const auto before = next.json();
            next.moveSite(key_, marker.row, maps_[key_].world(bytes.x(), bytes.y()));
            if (before != next.json()) {
                undo_->push(
                    new FieldEditCommand(project_, before, next.json(), "Move town or stronghold"));
                refresh(key_);
            }
        } else if (marker.kind == MapObject::Treasure) {
            auto next = *project_->world();
            auto r = ScenarioWorld::treasureRecord(next.scenario(key_).treasures, marker.row);
            put8(r, 1, quint8(bytes.x()));
            put8(r, 2, quint8(bytes.y()));
            next.setTreasure(key_, marker.row, r);
            const auto before = project_->world()->json(), after = next.json();
            if (before != after)
                undo_->push(new MapWorldCommand(project_, before, after));
        } else {
            auto next = *project_->scenarios();
            const bool squad = marker.kind == MapObject::Squad;
            auto r = squad ? next.scenario(key_).layout.deployments[marker.row]
                           : next.scenario(key_).layout.nodes[marker.row];
            put8(r, squad ? 3 : 4, quint8(bytes.x()));
            put8(r, squad ? 4 : 5, quint8(std::max(1, bytes.y())));
            if (squad)
                next.setDeployment(key_, marker.row, r, project_->classData());
            else
                next.setNode(key_, marker.row, r, project_->classData());
            commit_(next, squad ? "Move squad on map" : "Move waypoint on map");
        }
    } catch (const Error &e) {
        notice_->setText(QString::fromUtf8(e.what()));
    }
    canvas_->update();
}
void ScenarioMapEditor::changeBoundary(QRectF b) {
    if (loading_ || key_ < 1 || !project_->mapEdits())
        return;
    try {
        auto next = *project_->mapEdits();
        const auto before = next.json();
        next.setBoundary(key_, b);
        if (before != next.json()) {
            undo_->push(
                new FieldEditCommand(project_, before, next.json(), "Resize playable boundary"));
            refresh(key_);
        }
    } catch (const Error &e) {
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
void ScenarioMapEditor::paint(const QVector<QPointF> &stroke) {
    if (key_ < 1 || !project_->mapEdits())
        return;
    try {
        auto next = *project_->mapEdits();
        const auto before = next.json();
        const int type = tool_->currentIndex() == 3   ? 255
                         : tool_->currentIndex() == 2 ? 1
                                                      : paintType_->currentData().toInt();
        next.paint(key_, stroke, type, brushSize_->value());
        if (before == next.json())
            return;
        next.model(key_); // Check geometry and texture budgets before committing.
        undo_->push(new FieldEditCommand(project_, before, next.json(),
                                         type == 255 ? "Erase map painting"
                                         : type == 1 ? "Paint road"
                                                     : "Paint terrain"));
        refresh(key_);
    } catch (const Error &e) {
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
} // namespace lordly
