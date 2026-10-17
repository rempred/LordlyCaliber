#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QUndoStack;
namespace lordly {
class PersuasionEditor : public QWidget {
  public:
    PersuasionEditor(Project *, QUndoStack *, std::function<void(const Encounters &, const QString &)>);
    void refresh();
  private:
    void showGroup();
    void addLeader();
    void editGroup(bool create);
    void commit(const Persuasion &, const QString &);
    Project *project_;
    std::function<void(const Encounters &, const QString &)> commit_;
    QComboBox *group_;
    QLineEdit *search_;
    QLabel *members_, *notice_;
    QTableWidget *table_;
    QPushButton *add_, *create_, *edit_;
};
}
