#pragma once
#include "engine/project.h"
#include "engine/event_context.h"
#include <QWidget>
#include <functional>
#include <memory>
class QUndoStack;
class QComboBox;
class QCheckBox;
class QTreeWidget;
class QLabel;
class QPushButton;
class QFormLayout;
class QTreeWidgetItem;
class QStackedWidget;
class QTabBar;
namespace lordly {
class PendingEdits;
class EventFlowEditor;
class ScenarioFlowchart;
class ScenarioEventEditor : public QWidget {
  public:
    ScenarioEventEditor(Project *, QUndoStack *, std::function<void()>, QWidget *parent = nullptr);
    void selectScenario(int key);
    void selectLocation(int location);
    void showAllRows();
    void selectEventRow(int row, int offset = -1);
    void setRawInstructions(bool);
    void setCreateScenario(std::function<void(int)>);
    std::function<void(int)> editCutscene;
  protected:
    void showEvent(QShowEvent *) override;
  private:
    void refresh();
    void refreshContext();
    void populatePrograms(int preferredRow = -1);
    int selectedEntry() const;
    void showRow();
    void showChart();
    void updateView();
    void selectStep();
    void showParameters(QTreeWidgetItem *);
    void clearParameters();
    void apply();
    void rememberDraft();
    bool applyDrafts();
    void applyDraftTo(SceneEvents &, QPair<int, int>);
    struct Draft { int flag, baseFlag; bool value, baseValue; QString text; };
    QMap<QPair<int, int>, Draft> drafts_;
    QPair<int, int> selection_{-1, -1};
    PendingEdits *pending_ = nullptr;
    void commit(const SceneEvents &, const QString &);
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    int scenario_ = -1;
    int location_ = 6;
    bool loading_ = false;
    bool allRows_ = false;
    QComboBox *rows_, *flags_, *action_;
    QComboBox *locations_;
    QLabel *locationLabel_;
    QPushButton *createScenario_;
    std::function<void(int)> createScenarioCallback_;
    QTabBar *phases_;
    QCheckBox *otherPrograms_;
    std::unique_ptr<EventContexts> contexts_;
    QByteArray contextSignature_;
    const char *contextRom_ = nullptr;
    QComboBox *branchTarget_;
    QPushButton *applyBranch_;
    QPushButton *openCutscene_;
    QTreeWidget *steps_;
    QCheckBox *all_;
    QLabel *notice_, *detail_;
    QPushButton *apply_, *restore_, *jump_, *discard_;
    QWidget *flagPanel_, *branchPanel_;
    QWidget *parameterWidget_ = nullptr;
    QTreeWidgetItem *parameterItem_ = nullptr;
    QCheckBox *raw_;
    QStackedWidget *views_;
    EventFlowEditor *eventsView_;
    QTabBar *viewChoice_;
    ScenarioFlowchart *chart_;
    bool chartDirty_=true;
    QString chartKey_;
};
} // namespace lordly
