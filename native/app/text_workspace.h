#pragma once
#include "engine/project.h"
#include <QUndoStack>
#include <QWidget>
#include <functional>
class QComboBox;
class QLabel;
class QPushButton;
namespace lordly {
QWidget *createTextWorkspace(Project *project, QUndoStack *undo, std::function<void()> changed);
void editDescription(QWidget *parent, Project *project, QUndoStack *undo, const QString &kind,
                     int entry, std::function<void()> changed);
QPushButton *createActionDescriptionButton(QComboBox *actions, Project *project, QUndoStack *undo,
                                         std::function<void()> changed);
class DescriptionPanel final : public QWidget {
  public:
    DescriptionPanel(Project *, QUndoStack *, std::function<void()> changed, QWidget *parent = nullptr);
    void selectDescription(const QString &kind, int entry);
  private:
    void refresh();
    Project *project_;
    QString kind_;
    int entry_ = -1;
    QLabel *preview_;
    QPushButton *edit_;
};
}
