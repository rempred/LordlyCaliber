#pragma once
#include "engine/project.h"
#include <QUndoStack>
class QWidget;
namespace lordly {
void pushContentProject(Project *, QUndoStack *, Project, const QString &);
void deleteCustomEntry(QWidget *, Project *, QUndoStack *, const QString &, int);
void showCustomContent(QWidget *, Project *, QUndoStack *, const QString &kind = {});
}
