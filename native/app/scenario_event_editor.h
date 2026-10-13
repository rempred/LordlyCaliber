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
class ScenarioEventEditor : public QWidget {
  public:
    ScenarioEventEditor(Project *, QUndoStack *, std::function<void()>, QWidget *parent = nullptr);
    void selectScenario(int key);
  protected:
    void showEvent(QShowEvent *) override;
  private:
    void refresh();
    void showRow();
    void selectStep();
    void apply();
    void commit(const SceneEvents &, const QString &);
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    int scenario_ = -1;
    bool loading_ = false;
    QComboBox *rows_, *flags_, *action_;
    QTreeWidget *steps_;
    QCheckBox *all_;
    QLabel *notice_, *detail_;
    QPushButton *apply_, *restore_, *jump_;
};
} // namespace lordly
