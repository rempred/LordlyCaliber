#include "engine/scene_actors.h"
#include "game/director/actor_body.h"
#include "game/director/actor_roster.h"
#include <bit>
namespace lordly {
bool SceneActors::constructBriefing(int anchor, quint16 pose, bool slotsKnown, const BriefingCall *call) {
    checkSlot(anchor);
    if (slotsKnown && !actors_[anchor])
        return false; // No row can pass the wrapper's occupied-anchor test.
    // Blank class-appearance rows in the editor are not the caller's briefing roster.
    if (!binding_)
        throw Error("Briefing preview requires the 20 actor rows from this launch");
    for (int index = 0; index < 20; ++index) {
        const auto &row = bodyRow(index);
        const int source = qint32(u32(row, 0x48));
        if (!source || (u32(row, 0x40) & 0x300) != 0x200)
            continue;
        if (!actors_[anchor])
            throw Error("Briefing preview requires the launch's actor slots to identify the anchor");
        const bool hasClones = u32(row, 4) || u32(row, 8);
        if (!u32(row, 0)) {
            if (hasClones)
                throw Error("Briefing row has a clone without its first component");
            return false;
        }
        const int owner = qint32(u32(row, 0x4c)), mode = qint32(u32(row, 0x58)) < 4;
        int clone = 0;
        if (hasClones && lordly_body_classification(reinterpret_cast<const uint8_t *>(row.constData()), 0) == 1) {
            if (!slotsKnown)
                throw Error("Briefing preview requires the launch's actor slots to place a clone");
            while (clone < 28 && actors_[clone])
                ++clone;
            if (clone == 28)
                throw Error("Briefing clone has no free actor slot");
        }
        if (!call)
            throw Error("Briefing preview requires the preparation and allocation results for this command occurrence");
        const auto data = bodyData_ ? bodyData_ : BodyAnimationData::load(rom_);
        const auto items = data->items(rom_);
        const auto records = items.recordsWithPrefix();
        uint16_t carried[4], equipment;
        for (int i = 0; i < 4; ++i)
            carried[i] = u16(row, 0x36 + i * 2);
        if (lordly_body_equipment(reinterpret_cast<const uint8_t *>(records.constData()),
                                  int(items.entries().size()), carried, &equipment))
            throw Error("Briefing equipment exceeds the current item table");
        const int art = data->routes().equipmentArt(source, equipment);
        // Stage completed poses. A failed native allocation or blocked cache can leave a
        // partially written actor that the renderer cannot draw as a completed construction.
        QVector<QPair<int, ActorInstance>> prepared;
        QVector<BodyRegistration> registrations;
        int allocation = 0;
        for (int ordinal = 0; ordinal < 3; ++ordinal) {
            if (!u32(row, ordinal * 4))
                continue;
            const int slot = ordinal ? clone : anchor;
            if (ordinal) {
                if (allocation >= call->allocations.size())
                    throw Error("Briefing clone allocation result is missing");
                if (!call->allocations[allocation++])
                    throw Error("Briefing clone allocation failed");
            }
            if (prepared.size() >= call->preparations.size())
                throw Error("Briefing resource preparation result is missing");
            const auto &p = call->preparations[prepared.size()];
            if (p.source != source || p.owner != owner || p.flagA != mode || p.flagB != mode || p.equipment != art)
                throw Error("Briefing resource preparation differs from this actor's art, owner, orientation or equipment");
            if (p.status != "ready")
                throw Error("Briefing resource preparation reported " + p.status);
            auto record = ordinal ? prepared.last().second.record() : actors_[anchor]->record();
            if (ordinal) {
                put32(record, 0x11c, std::bit_cast<quint32>(std::bit_cast<float>(u32(record, 0x11c)) - 8.f));
                put32(record, 0x124, std::bit_cast<quint32>(std::bit_cast<float>(u32(record, 0x124)) + 8.f));
            }
            put32(record, 0xe4, slot);
            put32(record, 0xe8, source);
            put32(record, 0xec, owner);
            // The preceding actor's post-pose sequence cursor at 0xf0 is inherited.
            put32(record, 0xf4, 0);
            put32(record, 0xf8, 0);
            put16(record, 0x134, quint16(pose + 50 * ordinal));
            put16(record, 0x138, quint16(pose + 50 * ordinal));
            put16(record, 0x13a, mode);
            put8(record, 0x13d, 1);
            put8(record, 0x145, 1);
            put8(record, 0x146, mode);
            put8(record, 0x147, index);
            put8(record, 0x149, ordinal);
            ActorInstance actor(rom_, record, data);
            configureLinkedPose(actor);
            if (actor.pose().advance() != LORDLY_POSE_OK)
                throw Error(QString("Briefing component %1 pose %2: %3")
                                .arg(ordinal).arg(quint16(pose + 50 * ordinal)).arg(actor.pose().error()));
            prepared.append({slot, std::move(actor)});
            registrations.append({slot, source, owner, mode, mode, art});
        }
        for (auto &[slot, actor] : prepared)
            actors_[slot] = std::move(actor);
        registrations_ = std::move(registrations);
        return true;
    }
    return false;
}
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
        if (next.bodyConstruction(source) == 1)
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
        const bool privatePrimary =
            next.bodyData_ &&
            next.bodyData_->routes().privatePose(qint32(u32(actor, 0xe8)), qint32(u32(actor, 0xec)),
                                                 u8(actor, 0x146) * 2 + qint16(u16(actor, 0x13a)),
                                                 qint16(u16(actor, 0x138))) >= 0;
        const int target = lordly_roster_leader_slot(u32(row, 0x40), qint32(u32(row, 0x4c)),
                                                     qint16(u16(actor, 0x138)), privatePrimary);
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
