#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QUndoStack;
class QComboBox;
class QCheckBox;
class QTreeWidget;
class QLabel;
class QPushButton;
namespace lordly {
class PendingEdits;
class ScenarioEventEditor : public QWidget {
  public:
    ScenarioEventEditor(Project *, QUndoStack *, std::function<void()>, QWidget *parent = nullptr);
    void selectScenario(int key);
    void showAllRows();
    void selectEventRow(int row, int offset = -1);
    std::function<void(int)> editCutscene;
  protected:
    void showEvent(QShowEvent *) override;
  private:
    void refresh();
    void showRow();
    void selectStep();
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
    bool loading_ = false;
    bool allRows_ = false;
    QComboBox *rows_, *flags_, *action_;
    QComboBox *branchTarget_;
    QPushButton *applyBranch_;
    QPushButton *openCutscene_;
    QTreeWidget *steps_;
    QCheckBox *all_;
    QLabel *notice_, *detail_;
    QPushButton *apply_, *restore_, *jump_, *discard_;
};
} // namespace lordly
