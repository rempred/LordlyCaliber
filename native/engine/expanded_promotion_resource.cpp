#include "engine/expanded_promotion_resource.h"
#include <algorithm>

namespace lordly {
static bool filled(const QByteArray &bytes, char value) {
    return std::all_of(bytes.begin(), bytes.end(), [=](char byte) { return byte == value; });
}
QByteArray ExpandedPromotionResource::encode(const Promotion &promotion, const QByteArray &compiled,
                                             const ClassRuntimeInventory &inventory) {
    const auto data = promotion.buildExpandedData();
    if (compiled != data.bytes || inventory.classCount != data.classCount)
        throw Error("Expanded promotion code metadata and compiled tables disagree");
    const auto metadata = inventory.encode(), stream = encodeLzss(compiled);
    const quint32 stored = (stream.size() + 1) & ~qsizetype(1);
    const quint32 metadataOffset = (8 + stored + 15) & ~15U;
    if (quint64(metadataOffset) + metadata.size() > slotSize - 64)
        throw Error("Expanded promotion tables and metadata exceed their ROM allocation");
    QByteArray slot(slotSize, char(-1)), footer(64, 0);
    put32(slot, 0, stored + 4);
    put32(slot, 4, compiled.size());
    slot.replace(8, stored, QByteArray(stored, 3));
    slot.replace(8, stream.size(), stream);
    slot.replace(metadataOffset, metadata.size(), metadata);
    put32(footer, 0, 0x4c43464d);
    put32(footer, 4, 2);
    put32(footer, 8, stream.size());
    put32(footer, 12, compiled.size());
    put32(footer, 16, crc32(compiled));
    put32(footer, 20, data.classCount);
    put32(footer, 24, data.familyOffset);
    put32(footer, 28, data.maskOffset);
    put32(footer, 32, data.flagsOffset);
    put32(footer, 36, data.catalogOffset);
    put32(footer, 40, metadataOffset);
    put32(footer, 44, metadata.size());
    put32(footer, 48, crc32(metadata));
    put32(footer, 52, 1); // Current Army layout generation.
    put32(footer, 60, crc32(footer.first(60)));
    slot.replace(slotSize - 64, 64, footer);
    return slot;
}
ExpandedPromotionResource ExpandedPromotionResource::decode(const QByteArray &slot) {
    if (slot.size() != slotSize)
        throw Error("Truncated expanded promotion allocation");
    const auto footer = slot.last(64);
    const auto count = u32(footer, 20), logical = u32(footer, 8), size = u32(footer, 12);
    const auto metadataOffset = u32(footer, 40), metadataSize = u32(footer, 44);
    if (u32(footer, 0) != 0x4c43464d || u32(footer, 4) != 2 ||
        u32(footer, 60) != crc32(footer.first(60)) || u32(footer, 52) != 1 ||
        u32(footer, 56) != 0 || count < 165 || count > 256 || logical == 0 ||
        logical > slotSize - 72 || size > 65536 || u32(footer, 24) != count * 8 ||
        u32(footer, 28) != count * 12 || u32(footer, 32) != count * 13 ||
        u32(footer, 36) != ((count * 14 + 3) & ~3U) || size < u32(footer, 36) + 8)
        throw Error("Invalid expanded promotion descriptor");
    const quint32 stored = (logical + 1) & ~1U;
    if (u32(slot, 0) != stored + 4 || u32(slot, 4) != size || (metadataOffset & 15) ||
        metadataOffset < 8 + stored || metadataSize < 24 ||
        quint64(metadataOffset) + metadataSize > slotSize - 64 ||
        !filled(slot.mid(8 + logical, stored - logical), 3) ||
        !filled(slot.mid(8 + stored, metadataOffset - 8 - stored), char(-1)) ||
        !filled(
            slot.mid(metadataOffset + metadataSize, slotSize - 64 - metadataOffset - metadataSize),
            char(-1)))
        throw Error("Expanded promotion stream or metadata exceeds its declared bounds");
    const auto metadata = slot.mid(metadataOffset, metadataSize);
    if (crc32(metadata) != u32(footer, 48))
        throw Error("Expanded promotion runtime metadata is damaged");
    ExpandedPromotionResource result;
    result.inventory = ClassRuntimeInventory::decode(metadata);
    if (result.inventory.classCount != int(count))
        throw Error("Expanded promotion metadata has a different class count");
    qsizetype consumed = 0;
    result.data.bytes = decodeLzss(slot.mid(8, logical), size, &consumed);
    if (consumed != logical || crc32(result.data.bytes) != u32(footer, 16))
        throw Error("Expanded promotion tables failed their integrity check");
    result.data.classCount = count;
    result.data.familyOffset = count * 8;
    result.data.maskOffset = count * 12;
    result.data.flagsOffset = count * 13;
    result.data.catalogOffset = (count * 14 + 3) & ~3U;
    // Validate table/catalog agreement independently of the container. The ROM
    // loader must additionally preserve class 0 against its source baseline.
    Promotion validation;
    validation.originalGates = result.data.bytes.first(8);
    validation.originalMasks = result.data.bytes.mid(result.data.maskOffset, 1);
    validation.originalFlags = result.data.bytes.mid(result.data.flagsOffset, 1);
    validation.applyExpandedData(result.data.bytes, count);
    return result;
}
} // namespace lordly
