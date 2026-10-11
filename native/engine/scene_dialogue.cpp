#include "engine/scene_dialogue.h"
#include <cmath>
namespace lordly {
namespace {
constexpr quint32 initializer = 0x80198be8;
qint32 integerCoordinate(double value) {
    // Native projector returns float and trunc.w.s narrows to a signed word.
    const float native = float(value);
    if (!std::isfinite(native) || double(native) < -2147483648. || double(native) >= 2147483648.)
        throw Error("Dialogue projection exceeds the native coordinate range");
    return qint32(native);
}
} // namespace
void SceneDialogue::checkActor(int slot) {
    if (slot < 0 || slot >= 28)
        throw Error("Dialogue actor slot is outside 0..27");
}
const QByteArray &SceneDialogue::tracking(int slot) const {
    checkActor(slot);
    return tracking_[slot];
}
void SceneDialogue::supplyTracking(int slot, QByteArray record) {
    checkActor(slot);
    if (!record.isEmpty() && record.size() != 6)
        throw Error("Dialogue tracking requires its complete six-byte record");
    tracking_[slot] = std::move(record);
}
void SceneDialogue::setControl(int index, quint32 value) {
    if (index < 0 || index >= 8)
        throw Error("Dialogue control index is outside 0..7");
    controls_[index] = quint8(value);
}
quint8 SceneDialogue::control(int index) const {
    if (index < 0 || index >= 8)
        throw Error("Dialogue control index is outside 0..7");
    return controls_[index];
}
int SceneDialogue::find(quint8 window, const SceneResources &resources) {
    for (int slot = 0; slot < 6; ++slot) {
        const auto record = resources.record(slot);
        if ((u16(record, 0) & 0x8000) && u32(record, 0x10) == initializer &&
            u8(record, 0x8f) == window)
            return slot;
    }
    return -1;
}
int SceneDialogue::query(quint32 window, const SceneResources &resources) const {
    const int slot = find(quint8(window), resources);
    return slot < 0 ? (sharedStatus_ & 2 ? 3 : 1) : (u8(resources.record(slot), 0x8a) & 1) * 2;
}
void SceneDialogue::resume(quint32 window, SceneResources &resources) {
    const int slot = find(quint8(window), resources);
    if (slot < 0)
        return;
    auto record = resources.record(slot);
    put8(record, 0x8a, u8(record, 0x8a) | 2);
    put16(record, 0xe, 0xf000);
    resources.supplyRecord(slot, record);
}
void SceneDialogue::close(qint32 window, SceneResources &resources) {
    for (auto &record : tracking_) {
        if (!record.isEmpty() && qint16(u16(record, 2)) == window) {
            record.clear();
            break;
        }
    }
    const int slot = find(quint8(window), resources);
    if (slot < 0)
        return;
    auto record = resources.record(slot);
    put16(record, 0, u16(record, 0) | 0x1000);
    resources.supplyRecord(slot, record);
}
void SceneDialogue::place(SceneResources &resources, const Project &project) const {
    for (int actor = 0; actor < 28; ++actor) {
        const auto &tracking = tracking_[actor];
        if (tracking.isEmpty())
            continue;
        const int slot = qint16(u16(tracking, 0));
        auto record = resources.record(slot);
        if ((u8(record, 0x8a) & 0x40) || !(u16(record, 0) & 0x8000))
            continue;
        if (!project)
            throw Error("Actor-linked dialogue requires current actor projection");
        const auto point = project(actor);
        if (!point)
            continue; // Known empty Actor slot.
        const qint32 x = integerCoordinate(point->x()), y = integerCoordinate(point->y());
        const int threshold = qint16(quint32(qint16(u16(tracking, 4))) * 14U + 49U);
        const quint32 offset = float(point->y()) < float(threshold) ? 20U : 45U;
        put16(record, 0x8c, quint16(x));
        put16(record, 0x94, quint16(quint32(y) - offset));
        resources.supplyRecord(slot, record);
    }
}
void SceneDialogue::updatePlacement(SceneResources &resources, const Project &project) const {
    auto next = resources;
    place(next, project);
    resources = std::move(next);
}
int SceneDialogue::create(const QVector<quint32> &a, SceneResources &resources,
                          const Project &project, const Portrait &portrait) {
    if (a.size() != 13)
        throw Error("Dialogue window command requires thirteen operands");
    const int actor = qint32(a[3]);
    checkActor(actor);
    if (a[12] > 1)
        throw Error("Dialogue placement mode must be zero or one");
    quint32 face = a[10];
    if (face == 0xffffffffU) {
        if (!portrait)
            throw Error("Dialogue portrait requires its current actor identity");
        face = quint32(portrait(actor));
    }
    if (a[11] == 1)
        face = 0U - face;
    auto next = *this;
    auto pool = resources;
    next.controls_.fill(0);
    const int slot = pool.registerWindow(initializer, 0, 0, 0, 0);
    auto record = pool.record(slot);
    put16(record, 0x88, quint16(a[2]));
    quint8 flags = quint8(a[7]) ? 4 : 0;
    if (a[8] == 1)
        flags |= 0x20;
    if (a[9] == 1)
        flags |= 8;
    put8(record, 0x8b, a[12] ? 0 : quint8((a[5] << 4) + a[4]));
    put8(record, 0x8f, quint8(a[0]));
    put16(record, 0x96, quint16(a[1]));
    put16(record, 0x8c, 0);
    put16(record, 0x94, 0);
    put16(record, 0x92, quint16(face));
    if (a[12] == 1) {
        flags |= 0x40;
        put16(record, 0x8c, quint8(a[4]));
        put16(record, 0x94, quint16(a[5]));
    }
    put8(record, 0x8a, flags);
    pool.supplyRecord(slot, record);
    QByteArray tracking(6, 0);
    put16(tracking, 0, quint16(slot));
    put16(tracking, 2, quint16(a[0]));
    put16(tracking, 4, a[12] == 1 ? 0 : quint16(a[5]));
    next.tracking_[actor] = tracking;
    next.place(pool, project);
    resources = std::move(pool);
    *this = std::move(next);
    return slot;
}
} // namespace lordly
