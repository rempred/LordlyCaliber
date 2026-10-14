#include "app/scenario_map_editor.h"
#include "app/squad_order_editor.h"
#include "app/squad_route_steps.h"
#include "app/scenario_squad_panel.h"
#include "app/scenario_world_editor.h"
#include "app/view_position.h"
#include "engine/battlefield.h"
#include "engine/squad_members.h"
#include "engine/route_preview.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QImage>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QTabWidget>
#include <QScopedValueRollback>
#include <QKeyEvent>
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
    QPolygonF footprint;
    MountainPeak peak;
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
    QVector<QPair<int, QRectF>> labelHits;
    QVector<QPair<int, int>> edges;
    QVector<QPointF> route;
    struct StepMarker { int row; QString number; QPointF position; };
    QVector<StepMarker> stepMarkers;
    QVector<int> routeRows;
    QVector<QPair<int,QRectF>> stepHits;
    QVector<QPair<QString,QRectF>> triggerAreas;
    int selectedStep = -1;
    std::function<void(int)> stepSelected;
    bool routeReached = false;
    std::optional<QPointF> destination;
    bool draftRoute = false;
    std::function<void(int)> selected;
    std::function<void(int, QPointF)> moved;
    std::function<void(int, double)> heightMoved;
    bool addingPeak = false;
    std::function<void(QPointF)> peakPlaced;
    std::function<void()> cancelPeakPlacement;
    std::function<void(QPointF)> place;
    bool placing = false;
    bool pickingDestination = false;
    std::function<void(QPointF)> destinationPicked;
    std::function<void()> cancelDestination;
    int brushMode = 0;
    double brushRadius = 3;
    bool straightBrush = false;
    QPolygonF cityFootprint;
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
        if (drag_ >= 0 && drag_ < markers.size() && markers[drag_].kind == MapObject::Peak)
            setCursor(Qt::CrossCursor);
        drag_ = -1;
        dragging_ = false;
        movingPeak_ = false;
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
    double draggedHeight(int row, QPointF mouse) const {
        return std::clamp(markers[row].peak.height + (press_.y() - mouse.y()) / 24., 0., 8.);
    }
    QPointF peakBasePixel(int row) const {
        const auto delta = movingPeak_ && row == drag_ && dragging_ ? mouse_ - press_ : QPointF();
        return toPixel(*markers[row].position) + delta;
    }
    QPointF markerPixel(int row) const {
        const auto &m = markers[row];
        if (m.kind == MapObject::Peak) {
            const double height = row == drag_ && dragging_ && !movingPeak_
                                      ? draggedHeight(row, mouse_)
                                      : m.peak.height;
            return peakBasePixel(row) - QPointF(0, 12 + height * 24);
        }
        return row == drag_ && dragging_ ? mouse_ - dragOffset_ : toPixel(*m.position);
    }

  protected:
    void paintEvent(QPaintEvent *) override {
        labelHits.clear();
        stepHits.clear();
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
        for (int i=0;i<triggerAreas.size();++i) {
            const auto &[name,region]=triggerAreas[i];
            const QRectF box(toPixel(region.topLeft()),toPixel(region.bottomRight()));
            const QColor color=i?QColor("#d9a6ff"):QColor("#6de4ff");
            p.setPen(QPen(color,2,Qt::DashLine));p.setBrush(QColor(color.red(),color.green(),color.blue(),28));
            p.drawRect(box);p.fillRect(QRectF(box.topLeft(),QSizeF(p.fontMetrics().horizontalAdvance(name)+10,20)),QColor("#172534"));
            p.drawText(box.topLeft()+QPointF(5,15),name);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#678b98"), 1.5, Qt::DashLine));
        for (const auto &[a, b] : edges)
            if (markers[a].position && markers[b].position)
                p.drawLine(toPixel(*markers[a].position), toPixel(*markers[b].position));
        if (route.size() > 1) {
            QPolygonF line;
            for (auto point : route) line << toPixel(point);
            p.setPen(QPen(QColor(25, 22, 15, 220), 6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPolyline(line);
            const QColor color = routeReached ? QColor("#ffe080") : QColor("#ffae70");
            p.setPen(QPen(color, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPolyline(line);
            double since = 0;
            for (int i = 1; i < line.size(); ++i) {
                const auto d = line[i] - line[i - 1];
                const double length = std::hypot(d.x(), d.y());
                since += length;
                if (length < 1 || since < 65) continue;
                since = 0;
                const QPointF unit = d / length, normal(-unit.y(), unit.x());
                const QPointF tip = (line[i] + line[i - 1]) / 2;
                p.drawLine(tip, tip - unit * 8 + normal * 5);
                p.drawLine(tip, tip - unit * 8 - normal * 5);
            }
        }
        if(destination) {
            const auto point=toPixel(*destination);p.setPen(QPen(QColor("#6de4ff"),2));p.setBrush(QColor("#263844"));
            QPolygonF diamond;diamond<<point+QPointF(0,-9)<<point+QPointF(9,0)<<point+QPointF(0,9)<<point+QPointF(-9,0);p.drawPolygon(diamond);
            p.drawText(point+QPointF(12,-10),draftRoute?"Pending destination":"Order destination");
        }
        if(current>=0 && current<markers.size() && markers[current].kind==MapObject::Squad && markers[current].position) {
            const auto point=toPixel(*markers[current].position);p.setPen(QColor("#fff2a8"));p.drawText(point+QPointF(20,24),"Starting position");
        }
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
            const QPointF center = markerPixel(i);
            const qreal radius = m.radius();
            labelAreas << QRectF(center - QPointF(radius + 3, radius + 3),
                                 QSizeF((radius + 3) * 2, (radius + 3) * 2));
            p.setPen(
                QPen(i == current ? QColor("#fff2a8") : QColor("#0d131a"), i == current ? 3 : 1.5));
            p.setBrush(m.color);
            if (m.kind == MapObject::Peak) {
                const auto foot = peakBasePixel(i);
                labelAreas << QRectF(foot - QPointF(10, 8), QSizeF(20, 16));
                p.setPen(QPen(i == current ? QColor("#fff2a8") : m.color, 2));
                p.drawLine(foot, center);
                p.setBrush(QColor("#263021"));
                p.drawEllipse(foot, 5., 4.);
                p.setBrush(m.color);
                QPolygonF diamond;
                diamond << center + QPointF(0, -8) << center + QPointF(7, 0)
                        << center + QPointF(0, 8) << center + QPointF(-7, 0);
                p.drawPolygon(diamond);
            } else if (!m.icon.isNull()) {
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
            if(m.kind==MapObject::Waypoint && routeRows.contains(m.row))continue;
            if (!m.position)
                continue;
            const QPointF center = markerPixel(i);
            if (labels || i == current) {
                const QString text =
                    m.label.section(" · ", 0, 0) +
                    (m.kind == MapObject::Peak
                         ? QString(" · %1").arg(i == drag_ && dragging_ && !movingPeak_
                                                    ? draggedHeight(i, mouse_)
                                                    : m.peak.height,
                                                0, 'f', 2)
                         : QString());
                QRectF label(center + QPointF(m.radius() + 6, -10),
                             QSizeF(p.fontMetrics().horizontalAdvance(text) + 8, 20));
                for (int attempt = 0; attempt < 12; ++attempt) {
                    if (std::none_of(labelAreas.begin(), labelAreas.end(),
                                     [&](const QRectF &other) { return other.intersects(label); }))
                        break;
                    label.translate(0, 21);
                }
                labelAreas << label;
                labelHits << qMakePair(i, label);
                p.fillRect(label, QColor(12, 20, 28, 210));
                p.setPen(Qt::white);
                p.drawText(label.adjusted(4, 0, 0, 0), Qt::AlignVCenter, text);
            }
        }
        if (current >= 0 && current < markers.size() && !markers[current].footprint.isEmpty()) {
            QPolygonF outline;
            const auto delta = current == drag_ && dragging_ ? mouse_ - press_ : QPointF();
            for (auto v : markers[current].footprint)
                outline << toPixel(v) + delta;
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor("#ffe6a3"), 2));
            p.drawPolygon(outline);
        }
        if (brushMode == 5 && !cityFootprint.isEmpty() && underMouse()) {
            QPolygonF preview;
            const auto origin = fromPixel(mouse_);
            for (auto v : cityFootprint)
                preview << toPixel(origin + v);
            p.setPen(QPen(QColor("#ffe6a3"), 2));
            p.setBrush(QColor(255, 230, 163, 55));
            p.drawPolygon(preview);
            p.drawLine(mouse_, mouse_ + QPointF(0, -12));
        } else if (brushMode && underMouse()) {
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
        // Route badges sit above ordinary labels and select a step without leaving its squad.
        QVector<QRectF> badgeAreas;
        for(const auto &step:stepMarkers) {
            const auto anchor=toPixel(step.position);
            if(!rect().adjusted(-15,-15,15,15).contains(anchor.toPoint()))continue;
            const qreal width=std::max(24,p.fontMetrics().horizontalAdvance(step.number)+12);
            QRectF badge;
            for(int attempt=0;attempt<48;++attempt){
                const qreal dy=(attempt%2?-1:1)*((attempt+1)/2)*27;
                badge=QRectF(QPointF(std::clamp(anchor.x()+9,4.,this->width()-width-4),
                                    std::clamp(anchor.y()-32+dy,4.,qreal(height()-54))),QSizeF(width,24));
                if(std::none_of(badgeAreas.begin(),badgeAreas.end(),[&](const QRectF &r){return r.intersects(badge);}))break;
            }
            badgeAreas<<badge;stepHits<<qMakePair(step.row,badge);
            p.setPen(QPen(QColor("#6de4ff"),1.5));p.drawLine(anchor,badge.center());
            p.setBrush(step.row==selectedStep?QColor("#fff2a8"):QColor("#263844"));
            p.drawRoundedRect(badge,5,5);p.setPen(step.row==selectedStep?QColor("#241c10"):QColor("#ffffff"));
            p.drawText(badge,Qt::AlignCenter,step.number);
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
        if (pickingDestination && destinationPicked) {
            destinationPicked(fromPixel(e->position()));
            return;
        }
        if (placing && place) {
            place(fromPixel(e->position()));
            return;
        }
        if (addingPeak && peakPlaced) {
            peakPlaced(fromPixel(e->position()));
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
        // Labels are painted over markers. Resolve the topmost visible label
        // first, including labels displaced to avoid crowded icons. A label
        // click selects only; dragging the marker still moves the object.
        for(auto i=stepHits.crbegin();i!=stepHits.crend();++i)
            if(i->second.contains(press_)){cancelDrag();if(stepSelected)stepSelected(i->first);update();return;}
        for (auto i = labelHits.crbegin(); i != labelHits.crend(); ++i)
            if (i->second.contains(press_) && i->first >= 0 && i->first < markers.size()) {
                cancelDrag();
                current = i->first;
                if (selected)
                    selected(current);
                update();
                return;
            }
        const auto hit = markerHit(press_);
        current = hit.row;
        if (selected)
            selected(current);
        drag_ = current >= 0 && markers[current].movable() ? current : -1;
        movingPeak_ = hit.peakBase;
        dragOffset_ = drag_ >= 0 ? press_ - toPixel(*markers[drag_].position) : QPointF();
        updatePeakCursor();
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
        updatePeakCursor();
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
        const bool heightDrag =
            index >= 0 && markers[index].kind == MapObject::Peak && !movingPeak_;
        drag_ = -1;
        dragging_ = false;
        movingPeak_ = false;
        if (changed && heightDrag) {
            if (heightMoved)
                heightMoved(index, draggedHeight(index, e->position()));
        } else if (changed && moved)
            moved(index, fromPixel(e->position() - dragOffset_));
        updatePeakCursor();
        update();
    }
    void wheelEvent(QWheelEvent *e) override {
        const auto before = fromPixel(e->position());
        zoom_ = std::clamp(zoom_ * std::pow(1.0015, e->angleDelta().y()), .5, 8.);
        pan_ += e->position() - toPixel(before);
        update();
        e->accept();
    }
    void keyPressEvent(QKeyEvent *e) override {
        if (e->key() == Qt::Key_Escape && addingPeak && cancelPeakPlacement) {
            cancelPeakPlacement();
            return;
        }
        if (e->key() == Qt::Key_Escape && drag_ >= 0) {
            cancelDrag();
            update();
            return;
        }
        if (e->key() == Qt::Key_Escape && pickingDestination && cancelDestination) {
            cancelDestination();
            return;
        }
        QWidget::keyPressEvent(e);
    }

  private:
    struct MarkerHit {
        int row = -1;
        bool peakBase = false;
    };
    MarkerHit markerHit(QPointF point) const {
        MarkerHit hit;
        qreal nearest = std::numeric_limits<qreal>::max();
        auto consider = [&](int row, bool footprint) {
            const auto &m = markers[row];
            if (!m.position)
                return false;
            qreal distance = QLineF(markerPixel(row), point).length();
            bool base = false;
            if (m.kind == MapObject::Peak) {
                const qreal baseDistance = QLineF(peakBasePixel(row), point).length();
                const auto diamond = point - markerPixel(row), foot = point - peakBasePixel(row);
                const bool onDiamond = std::abs(diamond.x()) / 7 + std::abs(diamond.y()) / 8 <= 1,
                           onBase = foot.x() * foot.x() / 25 + foot.y() * foot.y() / 16 <= 1;
                // Keep the visible diamond's lower tip a height handle even at
                // zero height. Outside the shapes, use the nearest padded hit.
                base = !onDiamond && (onBase || (baseDistance <= 9 && baseDistance < distance));
                if (base)
                    distance = baseDistance;
                else if (distance > 11)
                    return false;
            } else if (distance > std::max(12., m.radius() + 2) &&
                       !(footprint && m.footprint.containsPoint(fromPixel(point), Qt::OddEvenFill)))
                return false;
            if (hit.row < 0 || m.layer() > markers[hit.row].layer() ||
                (m.layer() == markers[hit.row].layer() && distance <= nearest)) {
                hit = {row, base};
                nearest = distance;
            }
            return true;
        };
        // Keep the selected marker reachable when handles overlap.
        if (current >= 0 && current < markers.size() && consider(current, false))
            return hit;
        for (int i = 0; i < markers.size(); ++i)
            consider(i, true);
        return hit;
    }
    void updatePeakCursor() {
        if (markers.isEmpty() || markers.front().kind != MapObject::Peak || addingPeak || placing ||
            pickingDestination || panning_)
            return;
        const auto hit = drag_ >= 0 ? MarkerHit{drag_, movingPeak_} : markerHit(mouse_);
        setCursor(hit.row < 0    ? Qt::CrossCursor
                  : hit.peakBase ? Qt::SizeAllCursor
                                 : Qt::SizeVerCursor);
    }
    void extendStroke(QPointF p) {
        if (straightBrush && stroke.size() > 1)
            stroke.last() = p;
        else
            stroke << p;
    }
    qreal zoom_ = 1;
    QPointF pan_, mouse_, press_, dragOffset_;
    int drag_ = -1;
    bool dragging_ = false, panning_ = false, movingPeak_ = false;
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
    FieldEditCommand(Project *p, FieldEditDelta delta, const QString &label)
        : QUndoCommand(label), project_(p), delta_(std::move(delta)) {}
    void undo() override {
        project_->applyMapDelta(delta_, false);
    }
    void redo() override {
        project_->applyMapDelta(delta_, true);
    }

  private:
    Project *project_;
    FieldEditDelta delta_;
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
    tool_->addItems({"Select / move", "Paint terrain", "Draw road", "Erase painting",
                     "Edit boundary", "City pieces", "Mountain peaks"});
    painting->addWidget(tool_);
    paintType_ = new QComboBox;
    paintType_->setObjectName("mapPaintType");
    for (int type = 1; type <= 14; ++type)
        paintType_->addItem(Encounters::mapTerrainName(type), type);
    paintType_->setCurrentIndex(2);
    paintType_->setToolTip("Drag from one bridge end to the other for a straight span. Leave a gap "
                           "between separate bridges.");
    painting->addWidget(paintType_);
    brushLabel_ = new QLabel("Brush radius");
    painting->addWidget(brushLabel_);
    brushSize_ = new QDoubleSpinBox;
    brushSize_->setObjectName("mapBrushSize");
    brushSize_->setRange(1, 16);
    brushSize_->setDecimals(1);
    brushSize_->setSingleStep(.5);
    brushSize_->setValue(3);
    painting->addWidget(brushSize_);
    auto restore = new QPushButton("Restore map from opened ROM");
    restore->setObjectName("mapRestore");
    painting->addWidget(restore);
    painting->addStretch();
    layout->addWidget(paintTools_);
    peakTools_ = new QWidget;
    auto peakToolbar = new QHBoxLayout(peakTools_);
    peakToolbar->setContentsMargins(0, 0, 0, 0);
    peakToolbar->addWidget(new QLabel("Base: move · Diamond: height"));
    addPeak_ = new QPushButton("Add peak on map");
    addPeak_->setObjectName("mapPeakAdd");
    addPeak_->setCheckable(true);
    peakToolbar->addWidget(addPeak_);
    restorePeaks_ = new QPushButton("Restore all peaks");
    restorePeaks_->setObjectName("mapPeaksRestore");
    peakToolbar->addWidget(restorePeaks_);
    peakToolbar->addStretch();
    layout->addWidget(peakTools_);
    peakTools_->hide();
    cityTools_ = new QWidget;
    auto cityLayout = new QHBoxLayout(cityTools_);
    cityLayout->setContentsMargins(0, 0, 0, 0);
    cityLayout->addWidget(new QLabel("City pieces"));
    cityType_ = new QComboBox;
    cityType_->setObjectName("mapCityType");
    cityType_->setIconSize(QSize(48, 48));
    cityType_->setMinimumWidth(280);
    cityType_->setMinimumHeight(42);
    cityType_->setMaxVisibleItems(12);
    cityType_->addItem("Cobblestone paving", 32);
    cityType_->addItem("Cobblestone road", 33);
    cityType_->addItem("Romulus road", 34);
    cityType_->addItem("Talpaea road", 35);
    cityLayout->addWidget(cityType_, 1);
    cityLayout->addWidget(new QLabel("Rotation"));
    cityAngle_ = new QSpinBox;
    cityAngle_->setObjectName("mapCityRotation");
    cityAngle_->setRange(0, 359);
    cityAngle_->setSingleStep(15);
    cityAngle_->setSuffix("°");
    cityAngle_->setWrapping(true);
    cityLayout->addWidget(cityAngle_);
    auto cityHelp = new QLabel("Drag paving and roads. Click to place a building or wall.");
    cityHelp->setWordWrap(true);
    cityLayout->addWidget(cityHelp, 1);
    layout->addWidget(cityTools_);
    cityTools_->hide();
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
        box->setRange(1, 254);box->setKeyboardTracking(false);
        box->setObjectName("mapBoundary" + edgeNames[i]);
        boundaryFields_[i] = box;
        boundaryLayout->addWidget(box);
    }

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
    objectSearch_=new QLineEdit;objectSearch_->setObjectName("mapObjectSearch");objectSearch_->setPlaceholderText("Find object…");objectSearch_->setClearButtonEnabled(true);
    objectType_=new QComboBox;objectType_->setObjectName("mapObjectType");objectType_->addItem("All objects",-1);
    const QStringList types{"Squads","Waypoints","Towns and strongholds","Treasures","City scenery","Mountain peaks"};
    for(int i=0;i<types.size();++i)objectType_->addItem(types[i],i);
    auto objectFilter=new QHBoxLayout;objectFilter->addWidget(objectSearch_,1);objectFilter->addWidget(objectType_);
    side->addLayout(objectFilter);side->addWidget(objects_);
    connect(objectSearch_,&QLineEdit::textChanged,this,[this]{filterObjects();});
    connect(objectType_,&QComboBox::currentIndexChanged,this,[this]{filterObjects();});
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
    auto ordersPage = new QWidget; ordersPage->setObjectName("mapSquadOrders");
    auto ordersLayout = new QVBoxLayout(ordersPage); ordersLayout->setContentsMargins(0,0,0,0);
    ordersLayout->setAlignment(Qt::AlignTop);
    steps_=new SquadRouteSteps(project_,commit_);ordersLayout->addWidget(steps_);
    steps_->advanced=[this](int row){editFields_(MapObject::Waypoint,row);};
    steps_->selectLeader=[this](int row){selectSquad(row);};
    replaceOrders_=new QCheckBox("Replace entire route with one command…");
    replaceOrders_->setObjectName("mapReplaceRoute");ordersLayout->addWidget(replaceOrders_);
    orders_ = new SquadOrderEditor(project_, commit_);
    orders_->setObjectName("mapSimpleOrders");ordersLayout->addWidget(orders_);
    createRoute_=new QPushButton("Create step list from current order");createRoute_->setObjectName("mapCreateRoute");ordersLayout->addWidget(createRoute_);
    connect(createRoute_,&QPushButton::clicked,this,[this]{
        const int selected=objects_->currentRow();
        if(selected<0 || canvas_->markers[selected].kind!=MapObject::Squad)return;
        const int squad=canvas_->markers[selected].row;
        try {
            auto next=*project_->scenarios();
            next.setSquadCommand(key_,squad,{SquadBehavior::Guard},project_->classData());
            commit_(next,"Create squad route steps");
        } catch(const Error &e){notice_->setText(QString::fromUtf8(e.what()));}
    });
    connect(replaceOrders_,&QCheckBox::toggled,this,[this](bool on){orders_->setVisible(on || steps_->route().rows.isEmpty());});
    squadTabs_=new QTabWidget;squadTabs_->setObjectName("mapSquadTabs");
    squadTabs_->addTab(ordersPage,"Orders");
    squadPanel_=new ScenarioSquadPanel(project_,commit_,"mapSquad");
    squadTabs_->addTab(squadPanel_,"Members and supplies");fields->addWidget(squadTabs_);squadTabs_->hide();
    orders_->selectLeader=[this](int row){selectSquad(row);};
    routePreview_ = new QLabel;
    routePreview_->setObjectName("mapTerrainRoute");
    routePreview_->setWordWrap(true);
    routePreview_->setTextFormat(Qt::PlainText);
    ordersLayout->addWidget(routePreview_);
    auto legend=new QLabel("Numbers: route steps · Gold: planned route · Orange: partial route · Dashed: scripted links. Repeated numbers show possible positions after different branches.");
    legend->setWordWrap(true);legend->setObjectName("mapRouteLegend");ordersLayout->addWidget(legend);
    routePreview_->hide();
    auto beginPicking=[this, addTreasure](bool on) {
        if (on) {
            addTreasure->setChecked(false);
            tool_->setCurrentIndex(0);
            canvas_->cancelDrag();
            canvas_->setFocus();
        }
        canvas_->pickingDestination = on;
        canvas_->setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
        notice_->setText(on ? steps_->pickPrompt() : "");
        canvas_->update();
    };
    orders_->enableMapPicking([this,beginPicking](bool on){if(on)steps_->cancelPick();beginPicking(on);});
    steps_->picking=[this,beginPicking](bool on){if(on)orders_->cancelPick();beginPicking(on);};
    canvas_->cancelDestination = [this] { orders_->cancelPick();steps_->cancelPick(); };
    canvas_->destinationPicked = [this](QPointF point) {
        const int minimum=steps_->isPickingArea()?0:1;
        if (point.x() < minimum || point.x() > 255 || point.y() < minimum || point.y() > 255) {
            notice_->setText("Choose a point inside the mission map.");
            return;
        }
        const QPoint position(qRound(point.x()),qRound(point.y()));
        if(steps_->isPicking())steps_->setDestination(position);
        else orders_->setDestination(position);
    };
    canvas_->stepSelected=[this](int row){steps_->selectNode(row);};
    cityInspector_ = new QWidget;
    auto cityFields = new QHBoxLayout(cityInspector_);
    cityFields->setContentsMargins(0, 0, 0, 0);
    cityFields->addWidget(new QLabel("Rotation"));
    placedCityAngle_ = new QSpinBox;
    placedCityAngle_->setObjectName("mapPlacedCityRotation");
    placedCityAngle_->setRange(0, 359);
    placedCityAngle_->setSingleStep(15);
    placedCityAngle_->setSuffix("°");
    cityFields->addWidget(placedCityAngle_);
    auto removeCity = new QPushButton("Remove piece");placedCityAngle_->setKeyboardTracking(false);
    removeCity->setObjectName("mapCityRemove");
    cityFields->addWidget(removeCity);
    fields->addWidget(cityInspector_);
    cityInspector_->hide();
    connect(placedCityAngle_, &QSpinBox::valueChanged, this, [this] {
        if(loading_)return;
        const int row = objects_->currentRow();
        if (row < 0 || row >= canvas_->markers.size() ||
            canvas_->markers[row].kind != MapObject::City)
            return;
        const int index = canvas_->markers[row].row;
        editCity(index, project_->mapEdits()->changes(key_).city[index].position,
                 placedCityAngle_->value());
    });
    connect(removeCity, &QPushButton::clicked, this, [this] {
        const int row = objects_->currentRow();
        if (row < 0 || row >= canvas_->markers.size() ||
            canvas_->markers[row].kind != MapObject::City)
            return;
        try {
            auto next = *project_->mapEdits();
            const auto before = next;
            next.removeCity(key_, canvas_->markers[row].row);
            undo_->push(new FieldEditCommand(project_, before.difference(key_, next), "Remove city piece"));
            refresh(key_);
        } catch (const Error &e) {
            notice_->setText(QString::fromUtf8(e.what()));
        }
    });
    peakInspector_ = new QWidget;
    auto peakFields = new QVBoxLayout(peakInspector_);
    peakFields->setContentsMargins(0, 0, 0, 0);
    auto peakHelp =
        new QLabel("Drag the marker base to move the peak, or the diamond to change height. "
                   "Release to redraw the slopes. Escape cancels the drag.");
    peakHelp->setWordWrap(true);
    peakFields->addWidget(peakHelp);
    peakFields->addWidget(new QLabel("Height above ground"));
    peakHeight_ = new QDoubleSpinBox;
    peakHeight_->setObjectName("mapPeakHeight");
    peakHeight_->setRange(0, 8);
    peakHeight_->setDecimals(2);
    peakHeight_->setSingleStep(.1);
    peakHeight_->setKeyboardTracking(false);
    peakFields->addWidget(peakHeight_);
    restorePeak_ = new QPushButton("Restore peak height");
    restorePeak_->setObjectName("mapPeakRestore");
    peakFields->addWidget(restorePeak_);
    auto removePeak = new QPushButton("Remove peak");
    removePeak->setObjectName("mapPeakRemove");
    peakFields->addWidget(removePeak);
    fields->addWidget(peakInspector_);
    peakInspector_->hide();
    connect(peakHeight_, &QDoubleSpinBox::valueChanged, this, [this](double height) {
        if (!loading_)
            editPeak(objects_->currentRow(), height);
    });
    connect(restorePeak_, &QPushButton::clicked, this, [this] {
        const int row = objects_->currentRow();
        if (row >= 0 && row < canvas_->markers.size())
            editPeak(row, canvas_->markers[row].peak.defaultHeight);
    });
    canvas_->heightMoved = [this](int row, double height) { editPeak(row, height); };
    connect(removePeak, &QPushButton::clicked, this, [this] {
        const int row = objects_->currentRow();
        if (row < 0 || row >= canvas_->markers.size() ||
            canvas_->markers[row].kind != MapObject::Peak)
            return;
        try {
            auto next = *project_->mapEdits();
            next.removePeak(key_, canvas_->markers[row].peak);
            commitPeakEdit(next, "Remove mountain peak");
        } catch (const Error &e) {
            notice_->setText(QString::fromUtf8(e.what()));
        }
    });
    connect(addPeak_, &QPushButton::toggled, this, [this](bool on) {
        canvas_->cancelDrag();
        canvas_->addingPeak = on;
        canvas_->setCursor(Qt::CrossCursor);
        notice_->setText(on ? "Click inside Highlands to add a peak. Escape cancels."
                            : "Drag the base to move the peak, or the diamond to change height.");
        canvas_->update();
    });
    canvas_->cancelPeakPlacement = [this] { addPeak_->setChecked(false); };
    canvas_->peakPlaced = [this](QPointF position) {
        try {
            auto next = *project_->mapEdits();
            const auto peak = next.addPeak(key_, position);
            addPeak_->setChecked(false);
            commitPeakEdit(next, "Add mountain peak", peak);
        } catch (const Error &e) {
            notice_->setText(QString::fromUtf8(e.what()));
        }
    };
    connect(restorePeaks_, &QPushButton::clicked, this, [this] {
        auto next = *project_->mapEdits();
        next.restorePeaks(key_);
        commitPeakEdit(next, "Restore mountain peaks");
    });
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
    positionLabel_=new QLabel;positionLabel_->setWordWrap(true);fields->addWidget(positionLabel_);
    startPoint_=new QComboBox;startPoint_->setObjectName("mapPlacementPoint");fields->addWidget(startPoint_);
    connect(startPoint_,&QComboBox::activated,this,[this]{
        if(loading_ || key_<0)return;
        const int row=objects_->currentRow();if(row<0 || row>=canvas_->markers.size())return;
        const auto marker=canvas_->markers[row];const int point=startPoint_->currentData().toInt();
        if(!point){move(row,{x_->value(),z_->value()});return;}
        try {
            auto next=*project_->scenarios();const bool squad=marker.kind==MapObject::Squad;
            auto record=squad?next.scenario(key_).layout.deployments[marker.row]:next.scenario(key_).layout.nodes[marker.row];
            put8(record,squad?3:4,point);put8(record,squad?4:5,0);
            if(squad)next.setDeployment(key_,marker.row,record,project_->classData());else next.setNode(key_,marker.row,record,project_->classData());
            commit_(next,"Change scenario placement point");
        } catch(const Error &e){const auto message=QString::fromUtf8(e.what());select(row);notice_->setText(message);}
    });
    fields->addWidget(coordinates_);x_->setKeyboardTracking(false);z_->setKeyboardTracking(false);

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
    sidebar->setMinimumWidth(300);
    split->addWidget(sidebar);
    split->setSizes({700, 370});
    layout->addWidget(split, 1);
    notice_ = new QLabel;
    notice_->setObjectName("mapNotice");
    notice_->setWordWrap(true);
    layout->addWidget(notice_);
    canvas_->resizedBoundary = [this](QRectF b) { changeBoundary(b); };
    for(auto box:boundaryFields_)connect(box, &QSpinBox::valueChanged, this, [this] {
        if(loading_)return;
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
                if (mode) { orders_->cancelPick();steps_->cancelPick(); }
                canvas_->cancelDrag();
                canvas_->brushMode = mode < 4 ? mode : 0;
                cityTools_->setVisible(mode == 5);
                peakTools_->setVisible(mode == 6);
                addPeak_->setChecked(false);
                paintType_->setVisible(mode == 1);
                brushLabel_->setVisible(mode > 0 && mode < 4);
                brushSize_->setVisible(mode > 0 && mode < 4);
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
                if (mode == 5)
                    updateCityTool();
                else
                    canvas_->cityFootprint.clear();
                canvas_->setCursor(mode ? Qt::CrossCursor : Qt::ArrowCursor);
                if (!loading_)
                    refresh(key_);
            });
    paintType_->setEnabled(false);paintType_->hide();brushLabel_->hide();brushSize_->hide();
    connect(paintType_, &QComboBox::currentIndexChanged, this, [this] {
        canvas_->cancelDrag();
        const int terrain = paintType_->currentData().toInt();
        canvas_->straightBrush = tool_->currentIndex() == 1 && (terrain == 2 || terrain == 10);
        canvas_->update();
    });
    brushSize_->setEnabled(false);
    connect(cityType_, &QComboBox::currentIndexChanged, this, [this] { updateCityTool(); });
    connect(cityAngle_, &QSpinBox::valueChanged, this, [this] { updateCityTool(); });
    connect(brushSize_, &QDoubleSpinBox::valueChanged, this, [this](double n) {
        canvas_->brushRadius = n;
        canvas_->update();
    });
    connect(restore, &QPushButton::clicked, this, [this] {
        try {
            auto next = *project_->mapEdits();
            const auto before = next;
            next.restore(key_);
            if (!before.difference(key_, next).empty()) {
                undo_->push(new FieldEditCommand(project_, before.difference(key_, next),
                                                 "Restore map terrain and buildings"));
                refresh(key_);
            }
        } catch (const Error &e) {
            notice_->setText(QString::fromUtf8(e.what()));
        }
    });
    canvas_->selected = [this](int row) {
        if(row>=0 && row<objects_->count() && objects_->item(row)->isHidden()){objectSearch_->clear();objectType_->setCurrentIndex(0);}
        objects_->setCurrentRow(row);
    };
    canvas_->moved = [this](int row, QPointF position) {
        if (row < 0 || row >= canvas_->markers.size() || !maps_.contains(key_))
            return;
        const auto &m = maps_[key_];
        const QPointF world(m.bounds.left() + m.bounds.width() * position.x() / 256.,
                            m.bounds.top() + m.bounds.height() * position.y() / 256.);
        const auto kind = canvas_->markers[row].kind;
        if (kind == MapObject::Peak) {
            movePeak(row, position);
            return;
        }
        if (kind == MapObject::City) {
            const int index = canvas_->markers[row].row;
            editCity(index, world, project_->mapEdits()->changes(key_).city[index].rotation);
            return;
        }
        move(row, m.bytes(world, kind == MapObject::Squad || kind == MapObject::Waypoint));
    };
    connect(addTreasure, &QPushButton::toggled, this, [this](bool on) {
        if (on) {
            orders_->cancelPick();steps_->cancelPick();
            tool_->setCurrentIndex(0);
        }
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
    connect(objects_, &QListWidget::currentRowChanged, this, [this](int row) {
        if(loading_)return;select(row);
        if(row>=0 && row<canvas_->markers.size() && canvas_->markers[row].kind==MapObject::Squad && squadSelected)squadSelected(canvas_->markers[row].row);
    });
    for(auto box:{x_,z_})connect(box,&QSpinBox::valueChanged,this,[this]{if(!loading_)move(objects_->currentRow(),{x_->value(),z_->value()});});
    orders_->stateChanged=[this]{
        if(loading_)return;const int row=objects_->currentRow();
        if(row>=0 && row<canvas_->markers.size() && canvas_->markers[row].kind==MapObject::Squad)updateRoutePreview(canvas_->markers[row].row);
    };
    steps_->stateChanged=orders_->stateChanged;
    connect(fields_, &QPushButton::clicked, this, [this] {
        const int row = objects_->currentRow();
        if (row >= 0) {
            const auto marker = canvas_->markers[row];
            editFields_(marker.kind, marker.row);
        }
    });
}
void ScenarioMapEditor::refresh(int key) {
    orders_->cancelPick();
    steps_->cancelPick();
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
        canvas_->labelHits.clear();
        canvas_->edges.clear();
        select(-1);
        return;
    }
    const int previous = key_ == key ? objects_->currentRow() : -1;
    std::optional<MountainPeak> previousPeak;
    if (previous >= 0 && previous < canvas_->markers.size() &&
        canvas_->markers[previous].kind == MapObject::Peak)
        previousPeak = canvas_->markers[previous].peak;
    if (key_ != key) {
        canvas_->resetView();
        findChild<QPushButton *>("mapTreasureAdd")->setChecked(false);
        addPeak_->setChecked(false);
    }
    key_ = key;
    canvas_->cancelDrag();
    loading_ = true;
    QSignalBlocker block(objects_);
    objects_->clear();
    canvas_->markers.clear();
    canvas_->labelHits.clear();
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
                keys << (project_->scenarios()->entries().contains(k)?project_->scenarios()->scenario(k).name:QString("Scenario %1").arg(k));
            mapScope_->setText((keys.size()>1?"Shared map: ":"This mission's map: ")+keys.join(", ")+". Export updates the briefing image and minimap.");
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
        cityTools_->setEnabled(editable);
        peakTools_->setEnabled(editable);
        restorePeaks_->setEnabled(editable && !project_->mapEdits()->changes(key).peaks.isEmpty());
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
            item->setForeground(kind == MapObject::Peak ? QColor("#65431d") : color.darker(140));
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
        if (editable) {
            const auto &pieces = project_->mapEdits()->changes(key).city;
            for (int i = 0; i < pieces.size(); ++i) {
                const auto &asset = project_->mapEdits()->cityAsset(pieces[i].asset);
                add(MapObject::City, i, asset.name, pieces[i].position, QColor("#d7a44a"));
                const double a = pieces[i].rotation * 3.141592653589793 / 180.;
                for (auto p : {asset.bounds.topLeft(), asset.bounds.topRight(),
                               asset.bounds.bottomRight(), asset.bounds.bottomLeft()}) {
                    const QPointF rotated(p.x() * std::cos(a) - p.y() * std::sin(a),
                                          p.x() * std::sin(a) + p.y() * std::cos(a));
                    canvas_->markers.last().footprint << *normalize(pieces[i].position + rotated);
                }
                objects_->item(objects_->count() - 1)->setIcon(QPixmap::fromImage(asset.thumbnail));
            }
        }
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
        if (editable && tool_->currentIndex() == 6) {
            objects_->clear();
            canvas_->markers.clear();
            canvas_->edges.clear();
            const auto peaks = project_->mapEdits()->mountainPeaks(key);
            for (int i = 0; i < peaks.size(); ++i) {
                const auto &peak = peaks[i];
                const QPointF world(
                    map.bounds.left() + peak.position.x() * map.bounds.width() / 256,
                    map.bounds.top() + peak.position.y() * map.bounds.height() / 256);
                add(MapObject::Peak, i, QString("Peak %1").arg(i + 1), world, QColor("#ffe6a3"));
                canvas_->markers.last().peak = peak;
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
            cacheKey += ':' + QString::number(project_->mapEdits()->revision(key));
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
        notice_->setText(tool_->currentIndex() == 6
                             ? "Drag the base to move a peak inside Highlands. Drag the diamond to "
                               "change height. "
                               "Escape cancels."
                             : "Drag towns, strongholds, squads, waypoints or treasure to set "
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
        if (previousPeak)
            for (int i = 0; i < canvas_->markers.size(); ++i)
                if (canvas_->markers[i].kind == MapObject::Peak &&
                    canvas_->markers[i].peak.sameIdentity(*previousPeak)) {
                    objects_->setCurrentRow(i);
                    break;
                }
    } catch (const Error &e) {
        notice_->setText(QString::fromUtf8(e.what()));
    }
    loading_ = false;
    filterObjects();select(objects_->currentRow());
    updateCityTool();
    canvas_->update();
}
void ScenarioMapEditor::filterObjects() {
    for(int row=0;row<objects_->count();++row) {
        auto item=objects_->item(row);item->setHidden((objectType_->currentData().toInt()>=0 && item->data(Qt::UserRole).toInt()!=objectType_->currentData().toInt()) || !item->text().contains(objectSearch_->text(),Qt::CaseInsensitive));
    }
}
void ScenarioMapEditor::selectSquad(int row) {
    for(int i=0;i<canvas_->markers.size();++i)if(canvas_->markers[i].kind==MapObject::Squad && canvas_->markers[i].row==row) {
        if(objects_->item(i)->isHidden()){objectSearch_->clear();objectType_->setCurrentIndex(0);}
        objects_->setCurrentRow(i);return;
    }
}
void ScenarioMapEditor::select(int row) {
    if (loading_)
        return;
    const QScopedValueRollback<bool> guard(loading_,true);
    canvas_->current = row;
    canvas_->update();
    const bool valid = row >= 0 && row < canvas_->markers.size();
    const bool movable = valid && canvas_->markers[row].movable();
    const bool city = valid && canvas_->markers[row].kind == MapObject::City;
    const bool peak = valid && canvas_->markers[row].kind == MapObject::Peak;
    peakInspector_->setVisible(peak);
    const bool squad = valid && canvas_->markers[row].kind == MapObject::Squad;
    squadTabs_->setVisible(squad);
    squadPanel_->refresh(squad?key_:-1,squad?canvas_->markers[row].row:-1);
    orders_->refresh(squad ? key_ : -1, squad ? canvas_->markers[row].row : -1);
    steps_->refresh(squad ? key_ : -1, squad ? canvas_->markers[row].row : -1);
    replaceOrders_->setVisible(!steps_->route().rows.isEmpty());
    orders_->setVisible(steps_->route().rows.isEmpty() || replaceOrders_->isChecked());
    createRoute_->setVisible(squad && steps_->route().rows.isEmpty() &&
        u8(project_->scenarios()->scenario(key_).layout.deployments[canvas_->markers[row].row],6)==1 &&
        steps_->route().leader==canvas_->markers[row].row);
    updateRoutePreview(squad ? canvas_->markers[row].row : -1);
    cityInspector_->setVisible(city);
    const bool worldObject = valid && (canvas_->markers[row].kind == MapObject::Site ||
                                       canvas_->markers[row].kind == MapObject::Treasure);
    world_->setVisible(worldObject);
    coordinates_->setVisible(
        valid && !peak &&
        (!worldObject || (canvas_->markers[row].kind == MapObject::Site && movable)));
    positionLabel_->setVisible(valid && !peak && (!worldObject || (canvas_->markers[row].kind==MapObject::Site && movable)));
    startPoint_->setVisible(valid && (squad || canvas_->markers[row].kind==MapObject::Waypoint));
    startPoint_->clear();startPoint_->addItem("Map coordinates",0);
    if(valid && (squad || canvas_->markers[row].kind==MapObject::Waypoint)) {
        const auto &m=canvas_->markers[row];const auto &scenario=project_->scenarios()->scenario(key_);
        const auto record=squad?scenario.layout.deployments[m.row]:scenario.layout.nodes[m.row];
        const int point=u8(record,squad?4:5)?0:u8(record,squad?3:4);
        for(int i=0;i<maps_[key_].points.size();++i){
            QString name=QString("Placement point %1").arg(i+1);
            if(project_->world() && i<project_->world()->siteCount(key_))name=project_->world()->townName(u16(project_->world()->scenario(key_).sites,1+i*4));
            startPoint_->addItem(name,i+1);
        }
        if(startPoint_->findData(point)<0)startPoint_->addItem(QString("Original placement point %1").arg(point),point);
        startPoint_->setCurrentIndex(startPoint_->findData(point));
    }
    fields_->setVisible(valid && !worldObject && !city && !peak && !squad);
    summary_->setVisible(valid && !squad && (!worldObject || canvas_->markers[row].kind == MapObject::Site));
    reference_->setVisible(!worldObject && canvas_->terrain.isNull());
    referenceName_->setVisible(!worldObject);
    x_->setEnabled(movable);
    z_->setEnabled(movable);
    positionLabel_->setText(squad ? "Starting position · Drag the squad or edit coordinates. This is separate from its order destination." : "Map position · Changes apply automatically.");
    fields_->setEnabled(valid);
    selection_->clear();
    summary_->clear();
    if (!valid) {
        selection_->setText(
            tool_->currentIndex() == 6
                ? "Select a peak, or use Add peak on map. Paint Highlands to add a mountain."
                : "Select a squad, waypoint, site, treasure or city piece on the map.");
        return;
    }
    const auto &m = canvas_->markers[row];
    selection_->setText(m.label.section(" · flag ", 0, 0));
    if (peak) {
        QSignalBlocker block(peakHeight_);
        peakHeight_->setValue(m.peak.height);
        restorePeak_->setEnabled(std::abs(m.peak.height - m.peak.defaultHeight) > .00001);
        summary_->setText(
            "Peaks in the same mountain connect with ridges automatically. "
            "Add, move, remove or change a peak to reshape them. "
            "This top-down view shows height through shading; the terrain outline stays fixed.");
    } else if (worldObject) {
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
    } else if (city) {
        const auto &piece = project_->mapEdits()->changes(key_).city[m.row];
        placedCityAngle_->setValue(piece.rotation);
        summary_->setText("City scenery · Drag to move, or change its rotation below. "
                          "Movement follows the terrain underneath. This piece does not add "
                          "a town or a mission event.");
    } else {
        summary_->setText("Route waypoint · connected paths are shown on the map.");
        fields_->setText("Edit route and conditions…");
    }
    z_->setMinimum(m.kind == MapObject::Treasure || m.kind == MapObject::Site || city ? 0 : 1);
    if (m.position) {
        x_->setValue(int(std::round(m.position->x())));
        z_->setValue(int(std::round(m.position->y())));
    } else {
        x_->setValue(128);
        z_->setValue(128);
    }
}
void ScenarioMapEditor::updateRoutePreview(int squad) {
    canvas_->route.clear();
    canvas_->triggerAreas=squad>=0?steps_->triggerAreas():QVector<QPair<QString,QRectF>>();
    canvas_->stepMarkers.clear();canvas_->stepHits.clear();canvas_->routeRows.clear();canvas_->selectedStep=steps_->selectedNode();
    canvas_->routeReached = false;canvas_->destination.reset();canvas_->draftRoute=false;
    routePreview_->setVisible(squad >= 0);
    routePreview_->clear();
    if (squad >= 0 && project_->mapEdits()) {
        auto scenarios=*project_->scenarios();const auto draft=orders_->previewCommand();
        try {
            if(draft){scenarios.setSquadCommand(key_,squad,*draft,project_->classData());canvas_->draftRoute=true;}
            const auto command=draft?draft:scenarios.squadCommand(key_,squad);
            if(command && int(command->behavior)>=int(SquadBehavior::Move) && (draft || steps_->route().rows.isEmpty())) {
                const auto &map=maps_[key_];const auto world=map.placement(command->x,command->z);
                if(world)canvas_->destination=QPointF((world->x()-map.bounds.left())*256/map.bounds.width(),(world->y()-map.bounds.top())*256/map.bounds.height());
            }
            const auto preview=RoutePreview::squad(project_->rom(),scenarios,project_->classData(),*project_->mapEdits(),key_,squad);
            canvas_->route=preview.points;canvas_->routeReached=preview.reachable;
            routePreview_->setText((draft?"Pending order preview — not applied\n":"")+preview.message+"\n"+preview.detail);
            routePreview_->setToolTip("Gold line: planned terrain route. Dashed lines: order connections.");
            const auto &s=project_->scenarios()->scenario(key_);const auto &route=steps_->route();const auto &map=maps_[key_];
            if(route.leader>=0 && !route.rows.isEmpty()) {
                canvas_->routeRows=route.rows;
                auto position=[&](int x,int z)->std::optional<QPointF>{
                    const auto p=map.placement(x,z);if(!p)return {};
                    return QPointF((p->x()-map.bounds.left())*256/map.bounds.width(),(p->y()-map.bounds.top())*256/map.bounds.height());
                };
                struct Visit{int id;std::optional<QPointF> at;};
                const auto &d=s.layout.deployments[route.leader];
                QList<Visit> pending{{u8(d,6),position(u8(d,3),u8(d,4))}};
                QSet<QString> seen,badges;
                while(!pending.isEmpty()) {
                    const auto visit=pending.takeFirst();const int row=visit.id-4;
                    if(!route.rows.contains(row))continue;
                    const QString identity=QString("%1:%2:%3").arg(row).arg(visit.at?visit.at->x():-1).arg(visit.at?visit.at->y():-1);
                    if(seen.contains(identity))continue;seen.insert(identity);
                    const auto &n=s.layout.nodes[row];auto at=visit.at;
                    auto badge=[&](std::optional<QPointF> p,QString number){
                        if(!p)return;const QString key=number+QString(":%1:%2").arg(p->x()).arg(p->y());
                        if(!badges.contains(key)){badges.insert(key);canvas_->stepMarkers<<MapCanvas::StepMarker{row,number,*p};}
                    };
                    if(u8(n,1)==1) {
                        for(int offset:{4,6,8}) {
                            if(!u8(n,offset))break;at=position(u8(n,offset),u8(n,offset+1));
                            const QString suffix=u8(n,6)?QString(QChar('a'+(offset-4)/2)):QString();
                            badge(at,QString::number(route.number(visit.id))+suffix);
                        }
                    } else badge(at,QString::number(route.number(visit.id)));
                    // An extended script can choose destinations not modeled here.
                    if(SquadRoute::extended(n,&s.layout))at.reset();
                    for(int next:SquadRoute::targets(n,&s.layout))pending<<Visit{next,at};
                }
            }
        } catch(const Error &e){routePreview_->setText("Order preview unavailable: "+QString::fromUtf8(e.what()));}
    } else if (squad >= 0) routePreview_->setText("Terrain route preview is unavailable for this map.");
    canvas_->update();
}
void ScenarioMapEditor::editPeak(int row, double height) {
    if (row < 0 || row >= canvas_->markers.size() || canvas_->markers[row].kind != MapObject::Peak)
        return;
    try {
        auto next = *project_->mapEdits();
        next.setPeakHeight(key_, canvas_->markers[row].peak, height);
        commitPeakEdit(next, "Change mountain peak height");
    } catch (const Error &e) {
        select(row);
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
void ScenarioMapEditor::movePeak(int row, QPointF position) {
    if (row < 0 || row >= canvas_->markers.size() || canvas_->markers[row].kind != MapObject::Peak)
        return;
    try {
        auto next = *project_->mapEdits();
        next.movePeak(key_, canvas_->markers[row].peak, position);
        commitPeakEdit(next, "Move mountain peak");
    } catch (const Error &e) {
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
void ScenarioMapEditor::commitPeakEdit(const MapEdits &next, const QString &label,
                                       std::optional<MountainPeak> selected) {
    const auto before = *project_->mapEdits();
    if (before.difference(key_, next).empty())
        return;
    undo_->push(new FieldEditCommand(project_, before.difference(key_, next), label));
    refresh(key_);
    if (selected)
        for (int i = 0; i < canvas_->markers.size(); ++i)
            if (canvas_->markers[i].kind == MapObject::Peak &&
                canvas_->markers[i].peak.sameIdentity(*selected)) {
                objects_->setCurrentRow(i);
                break;
            }
}
void ScenarioMapEditor::move(int row, QPoint bytes) {
    if (row < 0 || row >= canvas_->markers.size())
        return;
    const auto marker = canvas_->markers[row];
    if (!marker.movable())
        return;
    try {
        if (marker.kind == MapObject::City) {
            editCity(marker.row, maps_[key_].world(bytes.x(), bytes.y()),
                     project_->mapEdits()->changes(key_).city[marker.row].rotation);
        } else if (marker.kind == MapObject::Site) {
            auto next = *project_->mapEdits();
            const auto before = next;
            next.moveSite(key_, marker.row, maps_[key_].world(bytes.x(), bytes.y()));
            if (!before.difference(key_, next).empty()) {
                undo_->push(
                    new FieldEditCommand(project_, before.difference(key_, next), "Move town or stronghold"));
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
        const auto message=QString::fromUtf8(e.what());select(row);notice_->setText(message);
    }
    canvas_->update();
}
void ScenarioMapEditor::changeBoundary(QRectF b) {
    if (loading_ || key_ < 1 || !project_->mapEdits())
        return;
    try {
        auto next = *project_->mapEdits();
        const auto before = next;
        next.setBoundary(key_, b);
        if (!before.difference(key_, next).empty()) {
            undo_->push(
                new FieldEditCommand(project_, before.difference(key_, next), "Resize playable boundary"));
            refresh(key_);
        }
    } catch (const Error &e) {
        const auto message=QString::fromUtf8(e.what());refresh(key_);notice_->setText(message);
    }
}
void ScenarioMapEditor::paint(const QVector<QPointF> &stroke) {
    if (key_ < 1 || !project_->mapEdits())
        return;
    try {
        auto next = *project_->mapEdits();
        const auto before = next;
        const bool cityMode = tool_->currentIndex() == 5;
        if (cityMode && cityType_->currentData().metaType().id() == QMetaType::QString) {
            const auto p = stroke.last();
            const auto &b = maps_[key_].bounds;
            next.placeCity(key_, cityType_->currentData().toString(),
                           b.topLeft() + QPointF(p.x() * b.width() / 256, p.y() * b.height() / 256),
                           cityAngle_->value());
            next.model(key_);
            undo_->push(new FieldEditCommand(project_, before.difference(key_, next), "Place city piece"));
            refresh(key_);
            objects_->setCurrentRow(-1);
            notice_->setText("City piece placed. Click to place another, or choose Select / move.");
            return;
        }
        const int type = tool_->currentIndex() == 3   ? 255
                         : tool_->currentIndex() == 2 ? 1
                         : cityMode                   ? cityType_->currentData().toInt()
                                                      : paintType_->currentData().toInt();
        next.paint(key_, stroke, type, brushSize_->value());
        if (before.difference(key_, next).empty())
            return;
        next.model(key_); // Check geometry and texture budgets before committing.
        undo_->push(new FieldEditCommand(project_, before.difference(key_, next),
                                         type == 255 ? "Erase map painting"
                                         : type == 1 ? "Paint road"
                                                     : "Paint terrain"));
        refresh(key_);
    } catch (const Error &e) {
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
void ScenarioMapEditor::updateCityTool() {
    if (tool_->currentIndex() != 5 || !project_->mapEdits() || !maps_.contains(key_))
        return;
    try {
        if (cityType_->count() == 4) {
            QSignalBlocker blocked(cityType_);
            for (const auto &asset : project_->mapEdits()->cityAssets())
                cityType_->addItem(QPixmap::fromImage(asset.thumbnail), asset.name, asset.id);
        }
        canvas_->cancelDrag();
        canvas_->brushMode = 5;
        canvas_->straightBrush = false;
        canvas_->cityFootprint.clear();
        const bool object = cityType_->currentData().metaType().id() == QMetaType::QString;
        cityAngle_->setEnabled(object);
        brushSize_->setEnabled(!object);
        brushSize_->setVisible(!object);
        brushLabel_->setVisible(!object);
        if (object) {
            const auto &asset =
                project_->mapEdits()->cityAsset(cityType_->currentData().toString());
            const auto &b = maps_[key_].bounds;
            const double a = cityAngle_->value() * 3.141592653589793 / 180.;
            for (auto p : {asset.bounds.topLeft(), asset.bounds.topRight(),
                           asset.bounds.bottomRight(), asset.bounds.bottomLeft()})
                canvas_->cityFootprint
                    << QPointF((p.x() * std::cos(a) - p.y() * std::sin(a)) * 256 / b.width(),
                               (p.x() * std::sin(a) + p.y() * std::cos(a)) * 256 / b.height());
        }
        canvas_->update();
    } catch (const Error &e) {
        canvas_->brushMode = 0;
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
void ScenarioMapEditor::editCity(int row, QPointF position, int rotation) {
    try {
        auto next = *project_->mapEdits();
        const auto before = next;
        next.moveCity(key_, row, position, rotation);
        next.model(key_);
        if (!before.difference(key_, next).empty()) {
            undo_->push(
                new FieldEditCommand(project_, before.difference(key_, next), "Move or rotate city piece"));
            refresh(key_);
        }
    } catch (const Error &e) {
        const auto message=QString::fromUtf8(e.what());select(objects_->currentRow());notice_->setText(message);
    }
}
} // namespace lordly
