#include "engine/scene_actors.h"
#include "game/director/actor_roster.h"
namespace lordly {
void SceneActors::materializeRoster(qint32 first, qint32 second, int variant) {
    if (!rosterLaunch_)
        throw Error("Actor roster requires its launch context and scenario");
    if (variant < 0 || variant > 1)
        throw Error("Invalid Actor materialization variant");
    auto next = *this;
    next.registrations_.clear();
    next.rosterDraws_.clear();
    int32_t wanted[2] = {first, second};
    bool seenSpecial = false;
    for (int identity = 0; identity < 20; ++identity) {
        const auto row = next.bodyRow(identity);
        const auto flags = u32(row, 0x40);
        const int source = qint32(u32(row, 0x48));
        if (!lordly_roster_select(wanted, flags, source))
            continue;
        const auto &launch = *next.rosterLaunch_;
        if (launch.context == -3 || launch.context == -10) {
            if (!launch.roster)
                throw Error("Actor roster filtering requires current army member state");
            if (next.rosterMemberUnavailable(row, *launch.roster))
                continue;
        }
        const int depth = qint32(u32(row, 0x58));
        const auto created = next.spawnBodyRow(identity, seenSpecial, depth < 4);
        if (source == 0x87)
            seenSpecial = true;
        const int offset =
            lordly_roster_pose_offset(flags, depth, launch.context, launch.scenario, variant);
        for (int slot : created) {
            if (slot == -1)
                break; // Native constructor's three-entry result retains holes.
            const int pose =
                offset < 0 ? -1 : qint16(u16(next.actors_[slot]->record(), 0x138)) + offset;
            try {
                next.selectBodyInternal({slot, -1, -1, pose, -1, -1, 0, 1, presentation_});
            } catch (const Error &e) {
                throw Error(QString("Roster row %1, Actor slot %2, source %3, pose %4: %5")
                                .arg(identity)
                                .arg(slot)
                                .arg(source)
                                .arg(pose)
                                .arg(e.what()));
            }
            if (offset >= 0)
                next.rosterDraws_ << slot;
        }
    }
    // Re-read the current slot after every swap. An Actor can be visited twice.
    for (int slot = 0; slot < 28; ++slot) {
        if (!next.actors_[slot])
            continue;
        const auto actor = next.actors_[slot]->record();
        const auto row = next.bodyRow(u8(actor, 0x147));
        const int target = lordly_roster_leader_slot(u32(row, 0x40), qint32(u32(row, 0x4c)),
                                                     qint16(u16(actor, 0x138)));
        if (target < 0 || target == slot)
            continue;
        std::swap(next.actors_[slot], next.actors_[target]);
        next.actors_[target]->setSlot(target);
        if (next.actors_[slot])
            next.actors_[slot]->setSlot(slot);
    }
    *this = std::move(next);
}
} // namespace lordly
