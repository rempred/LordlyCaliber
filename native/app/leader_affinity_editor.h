#pragma once
#include "engine/project.h"
#include <QWidget>
#include <array>
#include <QVector>
#include <functional>
class QLabel;
class QSpinBox;
class QUndoStack;
class QPushButton;
class QTableWidget;
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
    QTableWidget *table_;
    QVector<QLabel *> effects_;
    QVector<QSpinBox *> strength_;
    QPushButton *retail_, *manage_;
};
}
