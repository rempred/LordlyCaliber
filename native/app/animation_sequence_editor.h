#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QComboBox;
class QLabel;
class QSpinBox;
class QTableWidget;
class QUndoStack;
class QPushButton;
namespace lordly {
class ActorPoseEditor;
class AnimationSequenceEditor : public QWidget {
    Q_OBJECT
  public:
    AnimationSequenceEditor(Project *, QUndoStack *, std::function<void()> changed,
                            QWidget *parent = nullptr);

  private:
    void refresh();
    void selectRecord();
    void editLayers();
    void guarded(const std::function<void()> &);
    void apply(AnimationSequences, const QString &);
    void edit(const QString &, const std::function<void(PoseBank &, int, int)> &);
    int selectedClass() const;
    AnimationDescriptor descriptor() const;
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    bool refreshing_ = false;
    QComboBox *classes_ = nullptr, *routes_ = nullptr, *poses_ = nullptr;
    QTableWidget *records_ = nullptr;
    QSpinBox *frame_ = nullptr, *ticks_ = nullptr;
    QLabel *notice_ = nullptr, *summary_ = nullptr;
    QWidget *edits_ = nullptr;
    QPushButton *create_ = nullptr, *remove_ = nullptr;
    ActorPoseEditor *preview_ = nullptr;
};
} // namespace lordly
