#pragma once
#include "engine/project.h"
class QApplication;
namespace lordly {
int runAgentWorkspace(QApplication &application, Project project, const QString &input);
}
