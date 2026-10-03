#pragma once
#include "engine/project.h"
#include <QUndoStack>
#include <QWidget>
#include <functional>
namespace lordly {
QWidget *createTextWorkspace(Project *project, QUndoStack *undo, std::function<void()> changed);
}
