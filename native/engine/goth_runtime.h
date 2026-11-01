#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <optional>
namespace lordly {
struct GothRuntime {
    static constexpr int offset = 0x1390f0, size = 2848, headerOffset = offset + size - 16;
    int low = 25, high = 50;
    bool reserve = false;
    static QJsonObject definition();
    static std::optional<GothRuntime> read(const Rom &);
};
}
