#pragma once
#include "engine/director.h"
#include "engine/scene_actors.h"
namespace lordly {
// Declared preview participants, inferred from scene requests and portraits.
// This is editor launch data, not a reconstruction of a player's active battle.
struct SceneParty {
    QVector<int> classes, encounter;
    bool deployed = false, leaderOnly = false, preserved = false;
    static std::optional<SceneParty> infer(const DirectorProgram &, int classCount);
    QString description() const;
    void supply(SceneActors &, const BodyAnimationData &, bool reset = false) const;
};
} // namespace lordly
