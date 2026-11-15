#pragma once
#include "engine/rom.h"
#include <QSet>
#include <optional>
namespace lordly {
struct CursorBoundary {
    // The game's scenario key is an unsigned byte. Keep a slot for every value,
    // including the added missions above 127; validate authored keys separately.
    static constexpr int keySlots = 256, headerSize = 64 + keySlots;
    QByteArray bytes;
    QSet<int> keys;
    quint32 address = 0;
    static std::optional<CursorBoundary> read(const Rom &);
    QByteArray canonical(const Rom &) const;
    void install(QByteArray &) const;
};
} // namespace lordly
