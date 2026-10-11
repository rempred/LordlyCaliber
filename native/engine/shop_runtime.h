#pragma once
#include "engine/resource.h"
#include <QJsonObject>
#include <optional>

namespace lordly {
struct ShopPlacement {
    static constexpr quint32 descriptorOffset = 0x27c4000, descriptorSize = 64;
    static constexpr quint32 rowSize = 176, rowCount = 36, dataSize = rowSize * rowCount;
    static constexpr quint32 producerOffset = 0x19bf18, producerSize = 172;
    static constexpr quint32 stockOffset = 0x19be40, stockSize = 432;
    static constexpr quint32 guardOffset = producerOffset + 72;
    static constexpr quint32 slabDelta = 0x8007d7a0;
    quint32 offset = 0;
    int version = 2;
    int count = rowCount;
    quint32 byteSize() const { return rowSize * count; }
    static std::optional<ShopPlacement> read(const Rom &rom);
    QByteArray canonicalReferences(const QByteArray &owner, qsizetype start) const;
};
QJsonObject shopRuntimeCatalog();
quint32 rebuiltShopInstruction(const QJsonObject &patch);
void verifyShopRuntime(const Rom &rom, int version);
} // namespace lordly
