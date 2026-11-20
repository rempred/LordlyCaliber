#pragma once
#include <QJsonArray>
#include <QWidget>
class QComboBox;
class QLabel;
class QLineEdit;
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
    QString connection() const;
    void showDetails();
    Project *project_;
    QUndoStack *undo_;
    QLineEdit *rom_, *projectPath_;
    QLabel *status_;
    QPlainTextEdit *task_, *detail_;
    QTreeWidget *edits_;
    QJsonArray displayedActivity_;
};
} // namespace lordly
