#pragma once
#include "engine/project.h"
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
namespace lordly {
class ScenarioEditor;
class ScenarioEventEditor;
class CampaignCanvas;
class CampaignEditor : public QWidget {
  public:
    CampaignEditor(Project *, QUndoStack *, std::function<void()>, QWidget *parent = nullptr);
    void selectNode(int node);
    void selectEvent(int row, int offset = -1);

  private:
    void refresh();
    void showNode();
    void showBranches();
    void refreshFlow();
    void openCutscene(int selector);
    void editRule(int row);
    void createNode();
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
    QComboBox *branchNode_, *icon_;
    QSpinBox *x_, *y_;
    QTableWidget *rules_;
    QTreeWidget *flow_;
    QSpinBox *flowChaos_;
    QLabel *notice_, *routeInfo_;
    StoryRoutes routes_;
    int preferredNode_ = 6;
    bool loading_ = false;
};
} // namespace lordly
