#pragma once
#include "engine/project.h"
#include "engine/scenario_map.h"
#include <QCache>
#include <QWidget>
#include <functional>
class QLabel;
class QComboBox;
class QCheckBox;
class QListWidget;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;
class QUndoStack;
namespace lordly {
enum class MapObject { Squad, Waypoint, Site, Treasure };
class MapCanvas;
class ScenarioWorldEditor;
class ScenarioMapEditor : public QWidget {
  public:
    ScenarioMapEditor(Project *project, QUndoStack *undo,
                      std::function<void(const Scenarios &, const QString &)> commit,
                      std::function<void(MapObject, int)> editFields, QWidget *parent = nullptr);
    void refresh(int key);

  private:
    void select(int row);
    void move(int row, QPoint bytes);
    Project *project_;
    QUndoStack *undo_;
    std::function<void(const Scenarios &, const QString &)> commit_;
    std::function<void(MapObject, int)> editFields_;
    QMap<int, ScenarioMap> maps_;
    QCache<QString, QImage> battlefieldImages_{128 * 1024};
    QMap<QString, QStringList> battlefieldNotices_;
    MapCanvas *canvas_;
    QListWidget *objects_;
    QLabel *notice_, *reference_, *referenceName_, *selection_, *summary_;
    QWidget *coordinates_;
    ScenarioWorldEditor *world_;
    QSpinBox *x_, *z_;
    QPushButton *apply_, *fields_;
    QCheckBox *terrain_;
    QWidget *paintTools_, *boundaryTools_;
    std::array<QSpinBox *, 4> boundaryFields_;
    void changeBoundary(QRectF);
    QComboBox *tool_, *paintType_;
    QDoubleSpinBox *brushSize_;
    QLabel *mapScope_;
    void paint(const QVector<QPointF> &stroke);
    int key_ = -1;
    bool loading_ = false;
};
} // namespace lordly
