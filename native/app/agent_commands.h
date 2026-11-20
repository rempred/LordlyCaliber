#pragma once
#include "engine/project.h"
namespace lordly {
QJsonArray runAgentCommands(Project &project, const QJsonArray &commands, const QString &library,
                            const QString &tools, bool &edited);
QJsonObject agentCapabilities();
} // namespace lordly
