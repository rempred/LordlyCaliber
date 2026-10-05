#pragma once
#include "engine/project.h"
#include <QUndoStack>
#include <QWidget>
#include <functional>
class QSpinBox;
class QLabel;
class QPushButton;
class QCheckBox;
namespace lordly {
class ToolsEditor : public QWidget {
    Q_OBJECT
  public:
    ToolsEditor(Project *, QUndoStack *, std::function<void()> changed, QWidget *parent = nullptr);

  private:
    void refresh();
    void setPercent(int);
    void setChaos(bool);
    void setDisplay(ToolLabel, bool);
    void setHighAttack(bool);
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    QSpinBox *percent_;
    QLabel *weights_, *notice_;
    QPushButton *restore_, *retail_;
    QCheckBox *chaos_;
    QPushButton *chaosRestore_;
    QLabel *chaosNotice_;
    QCheckBox *displayBoxes_[2];
    QPushButton *displayRestores_[2];
    QLabel *displayNotices_[2];
    QCheckBox *highAttack_;
    QPushButton *highAttackRestore_;
    QLabel *highAttackNotice_;
};
} // namespace lordly
