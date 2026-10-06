#pragma once
#include "engine/project.h"
#include "engine/scenario_map.h"
#include <QWidget>
#include <functional>
class QLabel;
class QCheckBox;
class QListWidget;
class QSpinBox;
class QPushButton;
class QUndoStack;
namespace lordly {
enum class MapObject { Squad, Waypoint, Site, Treasure };
class MapCanvas;
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
    MapCanvas *canvas_;
    QListWidget *objects_;
    QLabel *notice_, *reference_, *referenceName_, *selection_;
    QSpinBox *x_, *z_;
    QPushButton *apply_, *fields_;
    QCheckBox *terrain_;
    int key_ = -1;
    bool loading_ = false;
};
} // namespace lordly
