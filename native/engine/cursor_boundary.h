#pragma once
#include "engine/rom.h"
#include <QSet>
#include <optional>
namespace lordly {
struct CursorBoundary {
    static constexpr int headerSize = 192;
    QByteArray bytes;
    QSet<int> keys;
    quint32 address = 0;
    static std::optional<CursorBoundary> read(const Rom &);
    QByteArray canonical(const Rom &) const;
    void install(QByteArray &) const;
};
} // namespace lordly
