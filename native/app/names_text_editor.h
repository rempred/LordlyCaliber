#pragma once
#include <QWidget>
#include <functional>
class QComboBox;
class QLineEdit;
class QTableWidget;
class QLabel;
class QCheckBox;
class QUndoStack;
class QPushButton;
namespace lordly {
class Project;
class NamesTextEditor final : public QWidget {
  public:
    NamesTextEditor(Project *,QUndoStack *,std::function<void()> changed,QWidget *parent=nullptr);
  private:
    void refresh();
    void selection();
    void applyName();
    void addName();
    void removeName();
    void filter();
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    QComboBox *kind_;
    QLineEdit *search_,*text_;
    QTableWidget *table_;
    QLabel *scope_,*error_;
    QCheckBox *shared_;
    QPushButton *add_,*remove_;
    bool loading_=false;
};
}
