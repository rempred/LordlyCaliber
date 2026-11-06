#pragma once
#include "engine/project.h"
#include "engine/entry_conditions.h"
#include <QWidget>
#include <functional>
class QUndoStack;
class QTabWidget;
class QListWidget;
class QComboBox;
class QSpinBox;
class QLabel;
class QTableWidget;
class QTreeWidget;
class QLineEdit;
class QScrollArea;
namespace lordly {
class ScenarioEditor;
class ScenarioEventEditor;
class CampaignCanvas;
class DescriptionPanel;
class CampaignEditor : public QWidget {
  public:
    CampaignEditor(Project *, QUndoStack *, std::function<void()>, QWidget *parent = nullptr);
    void selectNode(int node);
    void selectEvent(int row, int offset = -1);

  private:
    void refresh();
    void showNode();
    void showBranches();
    void showBranchConditions();
    void changeBranch(std::function<void(SceneEvents &)>, const QString &);
    void refreshFlow();
    void openCutscene(int selector);
    void editRule(int row);
    void createNode();
    void createMission(int location = -1);
    void editUnlocks();
    void commit(Project after, const QString &label);
    void commitMap(const CampaignMap &, const QString &label);
    QString eventName(int row) const;
    QComboBox *eventPicker() const;
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    QTabWidget *tabs_, *story_;
    ScenarioEditor *scenarios_;
    ScenarioEventEditor *events_;
    CampaignCanvas *canvas_;
    QListWidget *nodes_, *roads_;
    QLineEdit *label_;
    DescriptionPanel *description_;
    QComboBox *branchNode_, *icon_;
    QSpinBox *x_, *y_;
    QTableWidget *rules_;
    QScrollArea *branchDetails_;
    QMap<int,EntryConditions> branchEntries_;
    QMap<int,QString> branchEntryErrors_;
    int branchDetailsNode_=-1;
    QTreeWidget *flow_;
    QSpinBox *flowChaos_;
    QLabel *notice_, *routeInfo_;
    StoryRoutes routes_;
    int preferredNode_ = 6;
    bool loading_ = false;
};
} // namespace lordly
