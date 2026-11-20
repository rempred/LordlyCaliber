#pragma once
class QWidget;
class QUndoStack;
namespace lordly {
class Project;
void showCodePatches(QWidget *parent, Project &project, QUndoStack &undo);
} // namespace lordly
