#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QUndoStack;
class QTimeEdit;
class QListWidget;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;
class QTableWidget;
class QPushButton;
namespace lordly {
class ScenarioSquadPanel;
class ScenarioRouteEditor;
class ScenarioMapEditor;
class ScenarioEditor : public QWidget {
  public:
    ScenarioEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                   QWidget *parent = nullptr);

  private:
    void refresh();
    void filter();
    void showScenario();
    void showSquad();
    void commit(const Scenarios &next, const QString &label);
    void change(const QString &label, const std::function<void(Scenarios &)> &edit);
    int key() const;
    ScenarioSquadPanel *squadPanel_;
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    QLineEdit *search_;
    QListWidget *scenarios_, *squads_;
    QWidget *details_, *squadDetails_;
    QTimeEdit *missionTime_;
    QComboBox *missionRewards_[2]{};
    QSpinBox *base_;
    QLabel *notice_;
    QPushButton *remove_;
    ScenarioRouteEditor *routes_ = nullptr;
    ScenarioMapEditor *map_ = nullptr;
    int editingKey_ = -1, editingRow_ = -1;
    bool loading_ = true;
};
} // namespace lordly
