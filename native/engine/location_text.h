#pragma once
#include "engine/resource.h"
#include <optional>

namespace lordly {
// The location help selector uses bank 11, record (location ID - 1).
// Its only resource reference is a data word in the shared help directory.
struct LocationTextPlacement {
    quint32 offset = 0, allocation = 0, decodedSize = 0;
    static constexpr quint32 descriptorOffset = 0x237cb4a, descriptorSize = 64;
    static constexpr quint32 keyBase = 0x594280, directoryWord = 0x66d78;
    static constexpr int recordCount = 48, retailAllocation = 2280;
    static std::optional<LocationTextPlacement> read(const Rom &);
    static QByteArray canonicalReferences(const Rom &, QByteArray, qsizetype start);
};
QByteArray linkLocationText(const Rom &, const QByteArray &compiled);
} // namespace lordly
