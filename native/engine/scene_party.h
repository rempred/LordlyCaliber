#pragma once
#include "engine/director.h"
#include "engine/scene_actors.h"
namespace lordly {
class SaveDocument;
// Declared preview participants, inferred from scene requests and portraits.
// This is editor launch data, not a reconstruction of a player's active battle.
struct SceneParty {
    struct SavedMember {
        int index, cell;
        QByteArray record;
    };
    QVector<int> classes, encounter;
    bool deployed = false, leaderOnly = false, preserved = false;
    QVector<SavedMember> savedMembers;
    static std::optional<SceneParty> infer(const DirectorProgram &, int classCount);
    QString description() const;
    void useSavedUnit(const SaveDocument &, int unit);
    void supply(SceneActors &, const BodyAnimationData &, bool reset = false) const;
};
} // namespace lordly
