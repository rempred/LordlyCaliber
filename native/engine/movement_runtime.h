#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <optional>
namespace lordly {
struct MovementPlacement {
    static constexpr int descriptorOffset = 0x27c5700, descriptorSize = 64;
    quint32 offset = 0, address = 0, size = 0, patches = 0, patchCount = 0;
    quint32 originalDataCrc = 0, originalClassHeaderCrc = 0;
    QByteArray data;
    QJsonObject model;
    static std::optional<MovementPlacement> read(const Rom &);
    QByteArray canonical(const Rom &) const;
    QByteArray descriptor() const;
};
} // namespace lordly
