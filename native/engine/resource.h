#pragma once
#include "engine/rom.h"
namespace lordly {
struct Resource {
    qsizetype romOffset = 0;
    QByteArray originalSlot, original, decoded;
    static Resource load(const Rom &rom, qsizetype offset, qsizetype allocation);
    QByteArray container(const QByteArray &compiled) const;
};
} // namespace lordly
