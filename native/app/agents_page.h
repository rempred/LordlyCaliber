#pragma once
#include <QJsonArray>
#include <QWidget>
class QPlainTextEdit;
class QTreeWidget;
class QUndoStack;
namespace lordly {
class Project;
class AgentsPage final : public QWidget {
  public:
    AgentsPage(Project *project, QUndoStack *undo, QWidget *parent = nullptr);
    void refresh();

  protected:
    void showEvent(QShowEvent *event) override;

  private:
    void showDetails();
    Project *project_;
    QUndoStack *undo_;
    QPlainTextEdit *detail_;
    QTreeWidget *edits_;
    QJsonArray displayedActivity_;
};
} // namespace lordly
