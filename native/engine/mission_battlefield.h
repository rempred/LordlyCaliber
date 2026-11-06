#pragma once
#include "engine/scenarios.h"
#include "engine/scenario_world.h"

namespace lordly {
// Immutable data captured when a mission is created. Later donor edits do not
// change its squads, deployment settings or scenario-local site descriptors.
class MissionBattlefield {
  public:
    static QJsonObject capture(const Rom &, const Scenarios &, const ScenarioWorld &,
                               int source, const QString &mode);
    static QByteArray prepare(const Rom &, int target, const QJsonObject &snapshot);
};
} // namespace lordly
