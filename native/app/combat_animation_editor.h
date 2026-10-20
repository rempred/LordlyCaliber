#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QComboBox;
class QLabel;
class QPushButton;
class QUndoStack;
namespace lordly {
class AnimationSequenceEditor;
class CombatAnimationEditor : public QWidget {
    Q_OBJECT
  public:
    CombatAnimationEditor(Project *, QUndoStack *, std::function<void()> changed,
                          QWidget *parent = nullptr);
  private:
    void refresh(bool assigned = false);
    void preview();
    int classId() const;
    int owner() const;
    int flags() const;
    int assignedPose(int mode) const;
    Project *project_;
    bool refreshing_ = false;
    QComboBox *classes_, *actions_, *side_, *mode_, *variant_, *animations_;
    QLabel *assignment_, *notice_;
    QPushButton *assign_, *replace_;
    AnimationSequenceEditor *sequence_ = nullptr;
};
} // namespace lordly
