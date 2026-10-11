#include "engine/actor_materials.h"
#include "game/director/actor_material.h"
namespace lordly {
void ActorMaterials::check(int slot) {
    if (slot < 0 || slot >= 28)
        throw Error("Actor material slot is outside 0..27");
}
const QByteArray &ActorMaterials::tint(int slot) const {
    check(slot);
    return tints_[slot];
}
const QByteArray &ActorMaterials::material(int slot) const {
    check(slot);
    return materials_[slot];
}
void ActorMaterials::supplyTint(int slot, QByteArray record) {
    check(slot);
    if (!record.isEmpty() && record.size() != 12)
        throw Error("Actor tint record must contain 12 bytes");
    tints_[slot] = std::move(record);
}
void ActorMaterials::supplyMaterial(int slot, QByteArray record) {
    check(slot);
    if (!record.isEmpty() && record.size() != 7)
        throw Error("Actor material record must contain seven bytes");
    materials_[slot] = std::move(record);
}
void ActorMaterials::startTint(const QVector<quint32> &args, const QSet<int> &occupied) {
    if (args.size() != 9)
        throw Error("Actor tint command is truncated");
    const bool all = args[0] == 0xffffffffU;
    const int first = all ? 0 : qint16(args[0]), end = all ? 28 : qint16(args[0] + 1U);
    if (first >= end)
        return;
    check(first);
    check(end - 1);
    if (all)
        for (int slot = first; slot < end; ++slot)
            if (!occupied.contains(slot) && !tints_[slot].isEmpty())
                throw Error("All-actor tint would leave an empty slot with a freed native record");
    QByteArray record(12, 0);
    lordly_tint_begin(reinterpret_cast<uint8_t *>(record.data()), args.constData() + 1);
    for (int slot = first; slot < end; ++slot)
        if (!all || occupied.contains(slot))
            tints_[slot] = record;
}
void ActorMaterials::startMaterial(const QVector<quint32> &args) {
    if (args.size() != 8)
        throw Error("Actor material command is truncated");
    const int slot = qint16(args[0]);
    check(slot);
    QByteArray record(7, 0);
    lordly_material_begin(reinterpret_cast<uint8_t *>(record.data()), args.constData() + 1);
    materials_[slot] = std::move(record);
}
bool ActorMaterials::active(int slot) const {
    const auto &record = tint(slot);
    return !record.isEmpty() && u16(record, 8) != u16(record, 10);
}
void ActorMaterials::releaseTint(int slot) {
    check(slot);
    tints_[slot].clear();
}
void ActorMaterials::releaseMaterial(int slot) {
    check(slot);
    materials_[slot].clear();
}
QByteArray ActorMaterials::render(QByteArray &actor) {
    if (actor.size() != 336)
        throw Error("Actor rendering requires a complete native record");
    const int slot = qint32(u32(actor, 0xe4));
    check(slot);
    auto next = tints_[slot];
    uint8_t rgb[]{u8(actor, 0x142), u8(actor, 0x143), u8(actor, 0x144)};
    uint32_t words[20];
    const int tintWords = lordly_tint_render(
        next.isEmpty() ? nullptr : reinterpret_cast<uint8_t *>(next.data()), rgb, words);
    if (tintWords < 0)
        throw Error("Actor tint progress has invalid arithmetic");
    const auto &m = materials_[u8(actor, 0xe7)];
    const int materialWords = lordly_material_render(
        m.isEmpty() ? nullptr : reinterpret_cast<const uint8_t *>(m.constData()), rgb,
        words + tintWords);
    QByteArray commands((tintWords + materialWords) * 4, 0);
    for (int i = 0; i < tintWords + materialWords; ++i)
        put32(commands, i * 4, words[i]);
    tints_[slot] = std::move(next);
    for (int i = 0; i < 3; ++i)
        put8(actor, 0x142 + i, rgb[i]);
    return commands;
}
} // namespace lordly
