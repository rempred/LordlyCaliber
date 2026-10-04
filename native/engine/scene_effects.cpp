#include "engine/scene_effects.h"
namespace lordly {
void SceneEffects::checkSlot(int slot) {
    if (slot < 0 || slot >= 20)
        throw Error("Scene color slot is outside 0..19");
}
void SceneEffects::supplyColor(int slot, QByteArray record) {
    checkSlot(slot);
    if (!record.isEmpty() && record.size() != 80)
        throw Error("Scene color record must contain 80 bytes");
    colors_[slot] = std::move(record);
}
const QByteArray &SceneEffects::colorRecord(int slot) const {
    checkSlot(slot);
    return colors_[slot];
}
// Portable reconstruction of ROM 29C1B8..29C530.
void SceneEffects::startColor(const QVector<quint32> &a) {
    if (a.size() != 8)
        throw Error("Scene color command is truncated");
    const int slot = qint32(a[7]);
    if (slot != -1)
        checkSlot(slot);
    QByteArray record(80, 0);
    put16(record, 0, quint16(a[6]));
    for (int c = 0; c < 3; ++c) {
        put8(record, 4 + c, quint8(a[c]));
        put8(record, 7 + c, quint8(a[c]));
        put8(record, 10 + c, quint8(a[3 + c]));
    }
    if (slot == -1)
        for (auto &color : colors_)
            color = record;
    else
        colors_[slot] = record;
    shared_ = {quint8(a[0]), quint8(a[1]), quint8(a[2]), 255, 0x5f};
}
void SceneEffects::advanceColors() {
    auto next = colors_;
    for (auto &record : next) {
        if (record.isEmpty())
            continue;
        const int duration = qint16(u16(record, 0)), progress = qint16(u16(record, 2));
        if (progress == duration) {
            for (int c = 0; c < 3; ++c)
                put8(record, 4 + c, u8(record, 10 + c));
        } else if (progress < duration) {
            if (!duration)
                throw Error("Scene color record has an invalid zero divisor");
            const int current = qint16(quint16(progress + 1));
            put16(record, 2, quint16(current));
            for (int c = 0; c < 3; ++c) {
                const int from = u8(record, 7 + c), to = u8(record, 10 + c);
                put8(record, 4 + c, quint8(from - (from - to) * current / duration));
            }
        }
    }
    colors_ = std::move(next);
    if (!colors_[0].isEmpty())
        for (int c = 0; c < 3; ++c)
            shared_[c] = u8(colors_[0], 4 + c);
}
void SceneEffects::supplySpecialBlack(QByteArray record) {
    if (!record.isEmpty() && record.size() != 4)
        throw Error("Special black-fade record must contain four bytes");
    special_ = std::move(record);
}
void SceneEffects::advanceSpecialBlack() {
    if (!special_.isEmpty() && qint16(u16(special_, 2)) < qint16(u16(special_, 0)))
        put16(special_, 2, quint16(u16(special_, 2) + 1U));
}
quint8 SceneEffects::specialBlackAlpha() const {
    if (special_.isEmpty())
        return 0;
    const int duration = qint16(u16(special_, 0)), progress = qint16(u16(special_, 2));
    if (progress >= duration)
        return 255;
    if (!duration)
        throw Error("Special black-fade record has an invalid zero divisor");
    return quint8(progress * 255 / duration);
}
bool SceneEffects::visibleAboveSpecialBlack(qint32 source) {
    // ROM 2AA608 tests Actor +E8 (art/class source), not its pose-state field.
    return (quint32(source) - 0x51U) < 3U || source == 0xa3;
}
bool SceneEffects::service(quint32 opcode, const QVector<quint32> &operands) {
    if (opcode == 0x33) {
        startColor(operands);
        return true;
    }
    if (opcode == 0xa9) {
        if (!operands.isEmpty())
            throw Error("Special black-fade command has no operands");
        special_ = QByteArray::fromHex("003c0000");
        return true;
    }
    return false;
}
} // namespace lordly
