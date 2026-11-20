#pragma once
#include "engine/project.h"

namespace lordly {
void validateAgentActivity(const QJsonValue &value);
void recordAgentEdits(const Project &before, Project &after, const QString &source,
                      const QStringList &actions = {});
QJsonArray agentEditRows(const Project &project);
} // namespace lordly
