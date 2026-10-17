#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QUndoStack;
namespace lordly {
class PersuasionEditor : public QWidget {
  public:
    PersuasionEditor(Project *, QUndoStack *, std::function<void(const Encounters &, const QString &)>);
    void refresh();
  private:
    void showSelection();
    void editGroup(bool leader, bool create);
    int selectedLeader() const;
    void commit(const Persuasion &, const QString &);
    Project *project_;
    std::function<void(const Encounters &, const QString &)> commit_;
    QLabel *notice_;
    QPlainTextEdit *members_;
    QTableWidget *table_;
    QPushButton *addLeader_, *editLeader_, *addTarget_, *editTarget_;
};
}
