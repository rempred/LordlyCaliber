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
class QLineEdit;
class QTabWidget;
class QHideEvent;
namespace lordly {
enum class MapObject { Squad, Waypoint, Site, Treasure, City, Peak };
class MapCanvas;
class ScenarioWorldEditor;
class SquadOrderEditor;
class SquadRouteSteps;
class ScenarioSquadPanel;
class ScenarioMapEditor : public QWidget {
  public:
    ScenarioMapEditor(Project *project, QUndoStack *undo,
                      std::function<void(const Scenarios &, const QString &)> commit,
                      std::function<void(MapObject, int)> editFields, QWidget *parent = nullptr);
    void refresh(int key);
    void selectSquad(int row);
    std::function<void(int)> squadSelected;

  private:
    void hideEvent(QHideEvent *event) override;
    void select(int row);
    void filterObjects();
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
    QLabel *routePreview_;
    void updateRoutePreview(int squad);
    QWidget *coordinates_;
    ScenarioWorldEditor *world_;
    SquadOrderEditor *orders_;
    SquadRouteSteps *steps_;
    QCheckBox *replaceOrders_;
    QPushButton *createRoute_, *addSquad_;
    ScenarioSquadPanel *squadPanel_;
    QTabWidget *squadTabs_;
    QLineEdit *objectSearch_;
    QComboBox *objectType_, *startPoint_;
    QLabel *positionLabel_;
    QSpinBox *x_, *z_;
    QPushButton *fields_;
    QCheckBox *terrain_;
    QWidget *paintTools_, *boundaryTools_;
    std::array<QSpinBox *, 4> boundaryFields_;
    void changeBoundary(QRectF);
    QComboBox *tool_, *terrainAction_, *sceneryAction_, *paintType_;
    int activeTool() const;
    void updateTool();
    QDoubleSpinBox *brushSize_;
    QLabel *brushLabel_;
    QWidget *cityTools_, *cityInspector_;
    QWidget *peakInspector_;
    QDoubleSpinBox *peakHeight_;
    QPushButton *restorePeak_;
    void editPeak(int row, double height);
    void movePeak(int row, QPointF position);
    void commitPeakEdit(const MapEdits &, const QString &,
                        std::optional<MountainPeak> selected = {});
    QWidget *peakTools_;
    QPushButton *addPeak_, *restorePeaks_;
    QComboBox *cityType_;
    QSpinBox *cityAngle_, *placedCityAngle_;
    void updateCityTool();
    void editCity(int row, QPointF position, int rotation);
    QLabel *mapScope_;
    void paint(const QVector<QPointF> &stroke);
    int key_ = -1;
    bool loading_ = false;
};
} // namespace lordly
