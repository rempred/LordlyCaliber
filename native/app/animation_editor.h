#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QUndoStack;
namespace lordly {
class AnimationEditor : public QWidget {
    Q_OBJECT
  public:
    AnimationEditor(Project *, QUndoStack *, std::function<void()> changed,
                    QWidget *parent = nullptr);
    ~AnimationEditor() override;
    void selectClass(int);

  private:
    void refresh();
    void refreshEntry();
    void refreshAction();
    void changeEntry(const QString &, const std::function<void(AnimationRoutes::Entry &)> &);
    void apply(const AnimationRoutes &, const QString &);
    void guarded(const std::function<void()> &);
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    int selected_ = 1, actionRow_ = -1;
    bool refreshing_ = false;
    QComboBox *classes_ = nullptr, *sources_ = nullptr, *action_ = nullptr;
    std::array<QSpinBox *, 4> handles_{};
    std::array<QSpinBox *, 2> speeds_{};
    std::array<QSpinBox *, 3> selectors_{};
    QTableWidget *actions_ = nullptr;
    QWidget *controls_ = nullptr;
    QPushButton *replace_ = nullptr, *remove_ = nullptr;
    QLabel *notice_ = nullptr, *details_ = nullptr;
};
} // namespace lordly
