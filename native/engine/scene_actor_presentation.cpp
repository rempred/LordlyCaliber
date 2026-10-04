#include "engine/class_data.h"
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
    if (member >= 2)
        throw Error("Actor presentation linked-object selector exceeds its pair");
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
} // namespace lordly
