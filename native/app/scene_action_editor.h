#pragma once
#include "engine/project.h"
#include "engine/scene_actions.h"
#include <QWidget>
#include <functional>
class QUndoStack;
class QListWidget;
class QFormLayout;
class QLabel;
class QPushButton;
namespace lordly {
class SceneActionEditor : public QWidget {
    Q_OBJECT
  public:
    SceneActionEditor(Project *, QUndoStack *, std::function<void()> changed,
                      QWidget *parent = nullptr);
    void selectScene(SceneId, int command);
    void selectCommand(int command);
  signals:
    void commandSelected(int command);
    void validityChanged();

  private:
    void refresh(int command);
    void showAction();
    void publishFields();
    void validity(bool valid);
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    SceneId key_;
    QVector<SceneAction> actions_;
    QListWidget *list_;
    QFormLayout *fields_;
    QLabel *notice_;
    QPushButton *discard_;
    bool editing_ = false;
};
} // namespace lordly
