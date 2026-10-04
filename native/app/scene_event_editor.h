#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QUndoStack;
class QLineEdit;
class QTableWidget;
class QComboBox;
class QLabel;
class QPushButton;
namespace lordly {
class SceneEventEditor : public QWidget {
    Q_OBJECT
  public:
    SceneEventEditor(Project *, QUndoStack *, std::function<void()> changed,
                     QWidget *parent = nullptr);

  private:
    void refresh();
    void select();
    void apply(bool restore);
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    QTableWidget *sites_;
    QLineEdit *search_;
    QComboBox *target_;
    QLabel *status_;
    QPushButton *assign_, *restore_;
};
} // namespace lordly
