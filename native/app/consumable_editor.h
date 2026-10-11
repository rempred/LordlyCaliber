#pragma once
#include "engine/project.h"
#include <QUndoStack>
#include <QWidget>
#include <functional>
class QLabel;
class QSpinBox;
class QCheckBox;
class QPushButton;
namespace lordly {
class ConsumableEditor : public QWidget {
  public:
    ConsumableEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                     QWidget *parent = nullptr);
    void selectConsumable(int id);

  private:
    void refresh();
    void apply(bool restore);
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    int selected_ = -1;
    QJsonObject definition_;
    QLabel *heading_, *target_, *notice_, *minimumLabel_, *maximumLabel_;
    QSpinBox *minimum_, *maximum_;
    QCheckBox *description_;
    QPushButton *apply_, *restore_;
};
} // namespace lordly
