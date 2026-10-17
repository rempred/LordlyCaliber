#pragma once
#include "engine/project.h"
#include <QWidget>
#include <array>
#include <functional>
class QLabel;
class QSpinBox;
class QUndoStack;
class QPushButton;
namespace lordly {
class LeaderAffinityEditor : public QWidget {
  public:
    LeaderAffinityEditor(Project *, QUndoStack *, std::function<void()> changed);
    void selectClass(int id);
  private:
    void refresh();
    void write(const LeaderAffinity &);
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    int selected_ = -1, leader_ = -1;
    QLabel *scope_, *notice_;
    std::array<QLabel *, 4> members_, effects_;
    std::array<QSpinBox *, 4> strength_;
    QPushButton *retail_;
};
}
