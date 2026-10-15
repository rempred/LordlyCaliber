#pragma once
#include "engine/rom.h"
#include <QStringList>

namespace lordly::WeaponStatus {
constexpr int offset = 27, mask = 0x2e, poison = 0x20;
inline bool isWeapon(const QByteArray &record) {
    const auto type = u8(record, 4);
    return (type >= 1 && type <= 13) || type == 24;
}
inline QString name(int flags) {
    QStringList names;
    if (flags & poison) names << "Poison";
    if (flags & 2) names << "Sleep";
    if (flags & 4) names << "Paralysis";
    if (flags & 8) names << "Petrification";
    return names.isEmpty() ? QString("None") : names.join(" + ");
}
inline int replace(int flags, int effect) {
    if (effect != 0 && effect != poison && effect != 2 && effect != 4 && effect != 8)
        throw Error("Choose one weapon status effect");
    return (flags & ~mask) | effect;
}
} // namespace lordly::WeaponStatus
