#include "engine/class_data.h"
#include "engine/resource.h"
#include "engine/scene_actors.h"
#include "game/director/actor_presentation.h"
#include <bit>
#include <cmath>
namespace lordly {
namespace {
int secondaryPose(const QByteArray &row, const QByteArray &actor) {
    const auto owner = u32(row, 0x4c);
    return (owner == 10 || owner == 25 || owner == 123) && u8(actor, 0x149) == 1 ? 50 : 0;
}
double nativeDouble(const Rom &rom, int offset) {
    return std::bit_cast<double>((quint64(u32(rom.bytes(), offset)) << 32) |
                                 u32(rom.bytes(), offset + 4));
}
} // namespace
void SceneActors::supplyBodyObject(quint32 address, QByteArray record) {
    if (!address || record.size() < 0x30)
        throw Error("A linked army object requires its native identity and coordinate fields");
    bodyObjects_.insert(address, std::move(record));
}
void SceneActors::supplyPresentationRecord(QByteArray record) {
    if (!record.isEmpty() && record.size() != 29)
        throw Error("Actor presentation requires its complete 29-byte record");
    presentationRecord_ = std::move(record);
}
std::array<float, 2> SceneActors::presentationTarget(const QByteArray &actor) const {
    const auto &row = bodyRow(u8(actor, 0x147));
    const int member = u8(actor, 0x149);
    if (member >= 3)
        throw Error("Actor presentation linked-object selector exceeds its three entries");
    const auto address = u32(row, member * 4);
    const auto found = bodyObjects_.constFind(address);
    if (!address || found == bodyObjects_.cend())
        throw Error(QString("Actor presentation needs army object at RAM %1")
                        .arg(address, 8, 16, QChar('0')));
    return {float(qint16(u16(*found, 0x28))), float(qint16(u16(*found, 0x2c)))};
}
float SceneActors::bodySpeed(int source, int owner, int direction) const {
    const auto classes = ClassData::load(rom_);
    if (source < 0 || source >= classes.entries().size() || owner < 0 || direction < 0 ||
        direction > 1)
        throw Error("Invalid actor movement source, owner or direction");
    const int at = 0x594280 + 0x315736;
    const auto size = u32(rom_.bytes(), at);
    if (size < 6 || quint64(at) + 4 + size > quint64(rom_.bytes().size()))
        throw Error("Body movement resource exceeds the ROM");
    const int begin = u16(rom_.bytes(), at + 6), end = u16(rom_.bytes(), at + 8);
    if (begin > end || quint32(end) > size || (end - begin) % 2 || source >= (end - begin) / 2 ||
        owner >= (end - begin) / 2)
        throw Error("Body movement source or owner exceeds the current speed table");
    return lordly_body_speed(u8(classes.entries()[source].record, 69), owner,
                             u8(rom_.bytes(), at + 4 + begin + source * 2 + direction),
                             u8(rom_.bytes(), at + 4 + begin + owner * 2 + direction));
}
void SceneActors::startReturn(int group) {
    // ROM 2ACFD8. The selector is a signed word; its signed remainder is retained.
    auto next = *this;
    if (next.presentationRecord_.isEmpty())
        next.presentationRecord_ = QByteArray(29, 4);
    next.presentationRecord_[28] = 0;
    next.registrations_.clear();
    const double tx = nativeDouble(rom_, 0x2a8b38), tz = nativeDouble(rom_, 0x2a8b40);
    if (!std::isfinite(tx) || !std::isfinite(tz))
        throw Error("Actor position tolerances are nonfinite");
    for (int slot = 0; slot < 28; ++slot) {
        if (!next.actors_[slot])
            continue;
        const auto actor = next.actors_[slot]->record();
        if (u8(actor, 0x147) == 255)
            continue;
        const auto row = next.bodyRow(u8(actor, 0x147));
        const int side = (u32(row, 0x40) >> 8) & 1, subgroup = (u32(row, 0x40) >> 9) & 1;
        if ((group < 3) != bool(side) || (group % 3 == 1 && !subgroup) ||
            (group % 3 == 2 && subgroup))
            continue;
        const auto target = next.presentationTarget(actor);
        auto &position = next.actors_[slot]->pose().actor().position;
        if (!std::isfinite(position.x) || !std::isfinite(position.z))
            throw Error("Actor presentation has nonfinite coordinates");
        if (lordly_actor_snap(&position, target[0], target[1], tx, tz) == 3)
            continue;
        next.selectBodyInternal({slot, qint32(u32(row, 0x48)), qint32(u32(row, 0x4c)),
                                 9 + secondaryPose(row, actor), side, side, 0, 1, presentation_});
        next.presentationRecord_[slot] = 1;
    }
    *this = std::move(next);
}
void SceneActors::returnToBodyPosition(int slot) {
    // ROM 2AB0F8 uses the actor's stored slot, not the loop index, for both calls.
    const auto record = actors_[slot]->record();
    const auto row = bodyRow(u8(record, 0x147));
    const auto target = presentationTarget(record);
    const int side = (u32(row, 0x40) >> 8) & 1;
    const float x = actors_[slot]->pose().actor().position.x;
    const int pose = (side ? x < target[0] : target[0] < x) ? 5 : 11;
    const int source = qint32(u32(row, 0x48)), owner = qint32(u32(row, 0x4c));
    const int speed = int(bodySpeed(source, owner, pose == 5 ? 0 : 1) * 555.f);
    const int destination = qint32(u32(record, 0xe4));
    checkSlot(destination);
    if (actors_[destination]) {
        const int status = lordly_actor_move(
            &actors_[destination]->pose().actor().position, &motions_[destination], &constants_,
            -1.f, -1.f, target[0], target[1], 0, speed, mode_,
            u8(actors_[destination]->record(), 0x145), nullptr, nullptr);
        if (status != LORDLY_MOTION_OK)
            throw Error("Actor return movement produced invalid arithmetic");
    }
    selectBodyInternal({destination, source, owner, pose + secondaryPose(row, record), side, side,
                        0, 1, presentation_});
}
void SceneActors::advancePresentation() {
    // ROM 2AD224. The native vector survives the sweep that marks its final slot
    // complete, then releases on the following sweep. State 5 skips variant +8.
    if (presentationRecord_.isEmpty())
        return;
    auto next = *this;
    next.registrations_.clear();
    bool active = false;
    for (int slot = 0; slot < 28; ++slot) {
        const int state = u8(next.presentationRecord_, slot);
        if (state == 4)
            continue;
        active = true;
        if (!next.actors_[slot])
            throw Error("Actor presentation requires its occupied slot");
        const auto actor = next.actors_[slot]->record();
        const auto row = next.bodyRow(u8(actor, 0x147));
        int pose = secondaryPose(row, actor);
        if (state == 1) {
            if (!next.actors_[slot]->pose().bodyBusy()) {
                next.presentationRecord_[slot] = 2;
                next.returnToBodyPosition(slot);
            }
        } else if (state == 2 || state == 5) {
            if (state == 5) {
                if (next.actors_[slot]->pose().bodyBusy())
                    continue;
            } else {
                if (next.motions_[slot].present && next.motions_[slot].remaining)
                    continue;
                if (!u8(next.presentationRecord_, 28))
                    pose += 8;
            }
            next.selectBodyInternal({slot, -1, -1, pose, -1, -1, 0, 1, presentation_});
            next.presentationRecord_[slot] = 4;
        }
    }
    if (!active)
        next.presentationRecord_.clear();
    *this = std::move(next);
}
bool SceneActors::presentationMemberUnavailable(const QByteArray &row) const {
    // ROM 2AD574. Native tables have distinct layouts for IDs below/above 100;
    // launch extraction supplies their two observed fields without aliasing RAM.
    if (!presentationLaunch_ || !presentationLaunch_->roster)
        throw Error("Actor entrance variant requires its current army roster state");
    return rosterMemberUnavailable(row, *presentationLaunch_->roster);
}
bool SceneActors::rosterMemberUnavailable(const QByteArray &row,
                                          const PresentationRoster &roster) const {
    if (roster.overrideFlag == 1)
        return false;
    const int side = (u32(row, 0x40) >> 8) & 1;
    const auto &members = roster.members[roster.sideGroups[side] < 30 ? 0 : 1];
    const auto found = members.constFind(u8(row, 0xf6));
    if (found == members.cend())
        throw Error("Actor entrance requires the selected army member's current state");
    return !found->value || (found->flags & 4);
}
std::array<int, 4> SceneActors::spawnBodyRow(int identity, bool previousSpecial, int direction) {
    // ROM 2AB810. Three row pointers, not just the primary/paired-secondary two.
    const auto row = bodyRow(identity);
    const int source = qint32(u32(row, 0x48)), owner = qint32(u32(row, 0x4c));
    int type = 0;
    if (owner == 1)
        type = 1;
    else if (owner == 10 || owner == 25 || owner == 123)
        type = 2;
    else if (source == 0x87)
        type = previousSpecial ? 4 : 0;
    else if (source == 0xa1 || source == 0x88)
        type = 2;
    const int side = direction == -1 ? int((u32(row, 0x40) >> 8) & 1) : direction;
    std::array<int, 4> created{-1, -1, -1, -1};
    for (int member = 0; member < 3; ++member) {
        const auto pointer = u32(row, member * 4);
        if (!pointer)
            continue;
        int slot = 0;
        while (slot < 28 && actors_[slot])
            ++slot;
        if (slot == 28)
            throw Error("Actor entrance needs more than 28 scene slots");
        QByteArray record(336, 0);
        put8(record, 0x147, quint8(identity));
        put8(record, 0x149, quint8(member));
        const auto target = presentationTarget(record);
        float y;
        if (type == 4)
            y = float(qint16(u16(bodyObjects_.value(pointer), 0x2a)));
        else {
            if (!height_)
                throw Error("Actor entrance construction requires terrain height");
            const float height = height_(2, target[0], target[1]);
            if (!std::isfinite(height) || double(height) < -2147483648. ||
                double(height) >= 2147483648.)
                throw Error("Actor entrance terrain height exceeds native integer range");
            y = float(qint16(qint32(height)));
        }
        const int state = (type == 2 && member == 1) || type == 4 ? 60 : 10;
        for (int i = 0; i < 16; ++i)
            put8(record, i, 255);
        put32(record, 0xe0, 0x8022f2cc);
        put32(record, 0xe4, quint32(slot));
        if (source != -1)
            put32(record, 0xe8, quint32(source));
        if (owner != -1)
            put32(record, 0xec, quint32(owner));
        put32(record, 0xf0, 0xffffffffU);
        for (int i = 0; i < 3; ++i) {
            put32(record, 0x104 + i * 4, std::bit_cast<quint32>(1.f));
            put8(record, 0x142 + i, 255);
        }
        put32(record, 0x11c, std::bit_cast<quint32>(target[0]));
        put32(record, 0x120, std::bit_cast<quint32>(y));
        put32(record, 0x124, std::bit_cast<quint32>(target[1]));
        put32(record, 0x130, std::bit_cast<quint32>(1.f));
        put16(record, 0x134, quint16(state));
        put16(record, 0x138, quint16(state));
        put16(record, 0x13a, quint16(side));
        put8(record, 0x13c, presentation_);
        put8(record, 0x13d, 1);
        put8(record, 0x145, 1);
        put8(record, 0x146, quint8(side));
        actors_[slot] = ActorInstance(rom_, record);
        created[member] = slot;
    }
    return created;
}
void SceneActors::startEntrance() {
    // ROM 2AAC60. The Director operand is consumed but unused.
    if (!presentationLaunch_)
        throw Error("Actor entrance requires its launch preset and variant");
    const int at = 0x594280 + 0x1a9045a;
    const auto size = u32(rom_.bytes(), at);
    if (size > 65536 || u32(rom_.bytes(), at + 4) > 65536)
        throw Error("Actor entrance presets exceed the resource budget");
    const auto presets = Resource::load(rom_, at, qsizetype(size) + 4).decoded;
    const int preset = presentationLaunch_->preset;
    if (preset < 0 || quint64(preset) * 8 + 8 > quint64(presets.size()))
        throw Error("Actor entrance preset exceeds its current resource");
    std::array<float, 4> coordinates;
    for (int i = 0; i < 4; ++i)
        coordinates[i] = float(qint16(u16(presets, preset * 8 + i * 2)));
    auto next = *this;
    if (next.presentationRecord_.isEmpty())
        next.presentationRecord_ = QByteArray(29, 4);
    next.presentationRecord_[28] = 1;
    next.presentationOpacity_ = 128;
    next.registrations_.clear();
    const double tx = nativeDouble(rom_, 0x2a8b38), tz = nativeDouble(rom_, 0x2a8b40);
    if (!std::isfinite(tx) || !std::isfinite(tz))
        throw Error("Actor position tolerances are nonfinite");
    for (int slot = 0; slot < 28; ++slot) {
        if (!next.actors_[slot])
            continue;
        const auto actor = next.actors_[slot]->record();
        if (u8(actor, 0x147) == 255)
            continue;
        const auto row = next.bodyRow(u8(actor, 0x147));
        const int offset = secondaryPose(row, actor), source = qint32(u32(row, 0x48));
        if (!offset && (source == 0x87 || source == 0xa1 || source == 0x88)) {
            next.presentationRecord_[slot] = 4;
            continue;
        }
        const int side = (u32(row, 0x40) >> 8) & 1;
        next.selectBodyInternal(
            {slot, source, qint32(u32(row, 0x4c)), 9 + offset, side, side, 0, 1, presentation_});
        const auto target = next.presentationTarget(next.actors_[slot]->record());
        auto &position = next.actors_[slot]->pose().actor().position;
        if (!std::isfinite(position.x) || !std::isfinite(position.z))
            throw Error("Actor entrance has nonfinite coordinates");
        next.presentationRecord_[slot] =
            lordly_actor_snap(&position, target[0], target[1], tx, tz) == 3 ? 5 : 1;
    }
    bool previousSpecial = false;
    for (int identity = 0; identity < 20; ++identity) {
        const auto row = next.bodyRow(identity);
        const int source = qint32(u32(row, 0x48)), owner = qint32(u32(row, 0x4c));
        if (!source)
            continue;
        if ((presentationLaunch_->variant == -3 || presentationLaunch_->variant == -10) &&
            next.presentationMemberUnavailable(row))
            continue;
        bool found = false, room = false;
        for (const auto &actor : next.actors_) {
            if (!actor)
                room = true;
            else if (u8(actor->record(), 0x147) == identity) {
                found = true;
                break;
            }
        }
        if (found || !room)
            continue;
        const int side = (u32(row, 0x40) >> 8) & 1;
        const auto created = next.spawnBodyRow(identity, previousSpecial);
        if (source == 0x87)
            previousSpecial = true;
        // The native result list retains holes; its caller stops at the first -1.
        for (const int slot : created) {
            if (slot == -1)
                break;
            next.presentationRecord_[slot] = 2;
            auto &actor = *next.actors_[slot];
            const auto target = actor.pose().actor().position;
            const int speed = int(next.bodySpeed(source, owner, 0) * 555.f);
            if (lordly_actor_move(&actor.pose().actor().position, &next.motions_[slot], &constants_,
                                  coordinates[side ? 0 : 2], coordinates[side ? 1 : 3], target.x,
                                  target.z, 0, speed, mode_, u8(actor.record(), 0x145), nullptr,
                                  nullptr) != LORDLY_MOTION_OK)
                throw Error("Actor entrance movement produced invalid arithmetic");
            const int pose = qint16(u16(actor.record(), 0x138)) == 10 ? 5 : 55;
            next.selectBodyInternal({slot, -1, -1, pose, -1, -1, 0, 1, presentation_});
        }
    }
    *this = std::move(next);
}
} // namespace lordly
