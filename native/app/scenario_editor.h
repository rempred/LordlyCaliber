#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QUndoStack;
class QListWidget;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;
class QTableWidget;
class QPushButton;
namespace lordly {
class SquadMembersEditor;
class ScenarioRouteEditor;
class ScenarioWorldEditor;
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
    void applySquad();
    void preview();
    void commit(const Scenarios &next, const QString &label);
    void change(const QString &label, const std::function<void(Scenarios &)> &edit);
    int key() const;
    SquadMembersEditor *memberEditor_;
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    QLineEdit *search_;
    QListWidget *scenarios_, *squads_;
    QWidget *details_, *squadDetails_;
    QSpinBox *base_, *x_, *z_, *phase_;
    QComboBox *placement_, *start_, *healing_, *fruit_, *revival_, *drop_;
    QCheckBox *allied_;
    QLabel *notice_, *placementHelp_;
    QPushButton *remove_;
    ScenarioRouteEditor *routes_ = nullptr;
    ScenarioWorldEditor *world_ = nullptr;
    ScenarioMapEditor *map_ = nullptr;
    int editingKey_ = -1, editingRow_ = -1;
    bool loading_ = true;
};
} // namespace lordly
