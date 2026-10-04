#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QUndoStack;
class QFormLayout;
class QComboBox;
class QLabel;
class QPushButton;
namespace lordly {
class SceneCommandEditor : public QWidget {
    Q_OBJECT
  public:
    SceneCommandEditor(Project *, QUndoStack *, std::function<void()> changed,
                       QWidget *parent = nullptr);
    void selectCommand(quint32 key, int command);
  signals:
    void programChanged();

  private:
    void refresh();
    void publish(const QString &label, const ScenePrograms &, bool merge = false);
    void validity(bool valid);
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    quint32 key_ = 0;
    int command_ = -1;
    bool editing_ = false;
    QFormLayout *fields_;
    QComboBox *kind_;
    QLabel *notice_, *description_;
    QPushButton *insert_, *remove_, *restore_, *discard_;
};
} // namespace lordly
