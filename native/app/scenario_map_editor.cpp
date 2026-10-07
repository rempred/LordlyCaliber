#include "app/scenario_map_editor.h"
#include "app/view_position.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace lordly {
struct MapMarker {
    MapObject kind;
    int row;
    QString label;
    std::optional<QPointF> position;
    QColor color;
    bool movable() const {
        return kind != MapObject::Site;
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
    int current = -1;
    bool grid = true, labels = true, showTerrain = true;
    QImage terrain;
    QRectF terrainBounds;
    void cancelDrag() {
        drag_ = -1;
        dragging_ = false;
        panning_ = false;
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
        for (int i : drawOrder) {
            const auto &m = markers[i];
            if (!m.position)
                continue;
            const QPointF center = i == drag_ && dragging_ ? mouse_ : toPixel(*m.position);
            p.setPen(QPen(i == current ? Qt::white : QColor("#0d131a"), i == current ? 3 : 1.5));
            p.setBrush(m.color);
            if (m.kind == MapObject::Site)
                p.drawRect(QRectF(center - QPointF(6, 6), QSizeF(12, 12)));
            else if (m.kind == MapObject::Treasure) {
                QPolygonF diamond;
                diamond << center + QPointF(0, -7) << center + QPointF(7, 0)
                        << center + QPointF(0, 7) << center + QPointF(-7, 0);
                p.drawPolygon(diamond);
            } else
                p.drawEllipse(center, m.kind == MapObject::Waypoint ? 4. : 7.,
                              m.kind == MapObject::Waypoint ? 4. : 7.);
            if (labels || i == current) {
                const QString text = m.label.section(" · ", 0, 0);
                QRectF label(center + QPointF(10, -10),
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
        int hit = -1;
        qreal distance = 12;
        // Keep a list-selected marker reachable when several share a position.
        if (current >= 0 && markers[current].position &&
            QLineF(toPixel(*markers[current].position), press_).length() <= 12)
            hit = current;
        else
            for (int i = 0; i < markers.size(); ++i)
                if (markers[i].position) {
                    const auto d = QLineF(toPixel(*markers[i].position), press_).length();
                    if (d <= distance) {
                        hit = i;
                        distance = d;
                    }
                }
        current = hit;
        if (selected)
            selected(hit);
        drag_ = hit >= 0 && markers[hit].movable() ? hit : -1;
        update();
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        if (panning_)
            pan_ += e->position() - mouse_;
        mouse_ = e->position();
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
        const int index = drag_;
        const bool changed = dragging_;
        drag_ = -1;
        dragging_ = false;
        if (changed && moved)
            moved(index, fromPixel(e->position()));
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
    qreal zoom_ = 1;
    QPointF pan_, mouse_, press_;
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
    grid->setChecked(true);
    labels->setChecked(true);
    toolbar->addWidget(grid);
    toolbar->addWidget(labels);
    terrain_ = new QCheckBox("Terrain");
    terrain_->setObjectName("mapTerrain");
    terrain_->setToolTip("Show the calibrated terrain reference. Alignment is approximate.");
    terrain_->setChecked(true);
    toolbar->addWidget(terrain_);
    toolbar->addStretch();
    layout->addLayout(toolbar);
    auto split = new QSplitter;
    canvas_ = new MapCanvas;
    split->addWidget(canvas_);
    auto sidebar = new QWidget;
    auto side = new QVBoxLayout(sidebar);
    objects_ = new QListWidget;
    objects_->setObjectName("mapObjects");
    side->addWidget(objects_, 1);
    selection_ = new QLabel;
    selection_->setWordWrap(true);
    side->addWidget(selection_);
    auto coords = new QHBoxLayout;
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
    side->addLayout(coords);
    apply_ = new QPushButton("Set map coordinates");
    apply_->setObjectName("mapApply");
    side->addWidget(apply_);
    fields_ = new QPushButton("Edit selected fields");
    fields_->setObjectName("mapEditFields");
    side->addWidget(fields_);
    referenceName_ = new QLabel;
    referenceName_->setWordWrap(true);
    referenceName_->setObjectName("mapTerrainStatus");
    side->addWidget(referenceName_);
    reference_ = new QLabel;
    reference_->setAlignment(Qt::AlignCenter);
    side->addWidget(reference_);
    sidebar->setMinimumWidth(260);
    sidebar->setMaximumWidth(360);
    split->addWidget(sidebar);
    split->setSizes({700, 300});
    layout->addWidget(split, 1);
    notice_ = new QLabel;
    notice_->setObjectName("mapNotice");
    notice_->setWordWrap(true);
    layout->addWidget(notice_);
    canvas_->selected = [this](int row) { objects_->setCurrentRow(row); };
    canvas_->moved = [this](int row, QPointF position) {
        if (row < 0 || row >= canvas_->markers.size() || !maps_.contains(key_))
            return;
        const auto &m = maps_[key_];
        const QPointF world(m.bounds.left() + m.bounds.width() * position.x() / 256.,
                            m.bounds.top() + m.bounds.height() * position.y() / 256.);
        move(row, m.bytes(world, canvas_->markers[row].kind != MapObject::Treasure));
    };
    connect(addTreasure, &QPushButton::toggled, this, [this](bool on) {
        canvas_->placing = on;
        canvas_->setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
        notice_->setText(on ? "Click the map to place a new treasure. Click Add treasure again to "
                              "cancel. Then use Edit selected fields to choose its item."
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
            notice_->setText("Treasure placed. Use Edit selected fields to choose its item, or "
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
    if (key < 1)
        return;
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
    terrain_->setEnabled(false);
    referenceName_->clear();
    notice_->clear();
    try {
        if (!maps_.contains(key))
            maps_[key] = ScenarioMap::load(project_->rom(), key);
        const auto &map = maps_[key];
        auto normalize = [&](std::optional<QPointF> point) -> std::optional<QPointF> {
            if (!point)
                return {};
            return QPointF((point->x() - map.bounds.left()) / map.bounds.width() * 256.,
                           (point->y() - map.bounds.top()) / map.bounds.height() * 256.);
        };
        auto add = [&](MapObject kind, int row, QString label, std::optional<QPointF> position,
                       QColor color) {
            canvas_->markers << MapMarker{kind, row, label, normalize(position), color};
            auto item =
                new QListWidgetItem(label + (position ? "" : " · unresolved placement"), objects_);
            item->setForeground(color.darker(140));
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
                u8(d, 0) & 128 ? QColor("#69c9f0") : QColor("#f08780"));
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
        if (project_->world()) {
            const auto &world = *project_->world();
            const auto &scene = world.scenario(key);
            for (int i = 0; i < world.siteCount(key); ++i) {
                const int id = u16(scene.sites, 1 + i * 4);
                add(MapObject::Site, i, world.townName(id), map.placement(i + 1, 0),
                    QColor("#c9b4eb"));
            }
            for (int i = 0; i < ScenarioWorld::treasureCount(scene.treasures); ++i) {
                const auto r = ScenarioWorld::treasureRecord(scene.treasures, i);
                add(MapObject::Treasure, i,
                    QString("Treasure %1 · flag %2").arg(i + 1).arg(u8(r, 0)),
                    map.world(u8(r, 1), u8(r, 2)), QColor("#f4cc6c"));
            }
        }
        if (map.image) {
            const auto &r = *map.image;
            QImage image(reinterpret_cast<const uchar *>(r.rgba.constData()), r.width, r.height,
                         r.width * 4, QImage::Format_RGBA8888);
            reference_->setPixmap(QPixmap::fromImage(image).scaled(260, 170, Qt::KeepAspectRatio,
                                                                   Qt::FastTransformation));
            if (const auto bounds = map.imageGridBounds()) {
                canvas_->terrain = image.copy();
                canvas_->terrainBounds = *bounds;
                terrain_->setEnabled(true);
            }
            referenceName_->setText("ROM terrain · " + map.imageName +
                                    (map.imageBounds
                                         ? "\nCalibrated alignment (approximate)."
                                         : "\nNo calibrated alignment for this image."));
        } else
            referenceName_->setText("This scenario has no ROM map image.");
        notice_->setText("Initial placements · Drag squads, waypoints or treasure to set "
                         "coordinates. Selecting a point-based object "
                         "and moving it changes its placement to map coordinates. " +
                         map.placementNotice);
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
    x_->setEnabled(movable);
    z_->setEnabled(movable);
    apply_->setEnabled(movable);
    fields_->setEnabled(valid);
    selection_->clear();
    if (!valid)
        return;
    const auto &m = canvas_->markers[row];
    selection_->setText(m.label +
                        (m.movable() ? "" : "\nSite position comes from the scenario geometry."));
    z_->setMinimum(m.kind == MapObject::Treasure ? 0 : 1);
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
        if (marker.kind == MapObject::Treasure) {
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
} // namespace lordly
