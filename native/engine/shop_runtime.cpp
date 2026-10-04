#include "engine/shop_runtime.h"
#include "engine/class_data.h"
#include "engine/promotion.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace lordly {
QJsonObject shopRuntimeCatalog() {
    QFile file(":/game/shops/owners.json");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing shop runtime catalog");
    return QJsonDocument::fromJson(file.readAll()).object();
}
quint32 rebuiltShopInstruction(const QJsonObject &patch) {
    const auto word = quint32(patch["original"].toDouble());
    const auto kind = patch["kind"].toString();
    if (kind == "nop")
        return 0;
    if (kind == "guardCall")
        return 0x0c000000U |
               (((ShopPlacement::producerOffset + ShopPlacement::slabDelta + 72) >> 2) & 0x3ffffff);
    if (kind == "guardArgs")
        return (2U << 21) | (4U << 16) | (4U << 11) | 36U;
    if (kind == "guardBranch")
        return (4U << 26) | (2U << 21) | ((0x197f18 - 0x197d74 - 4) / 4);
    if (kind == "store")
        return (word & 0x03ffffffU) | (41U << 26); // sh
    if (kind == "load")
        return (word & 0x03ffffffU) | (37U << 26); // lhu
    if (kind == "index")
        return (word & ~0x7c0U) | (1U << 6);
    if (kind == "offset" || kind == "stride")
        return word - 2;
    throw Error("Unrecognized shop instruction transformation");
}
QByteArray ShopPlacement::canonicalReferences(const QByteArray &owner, qsizetype start) const {
    auto result = owner;
    for (const auto &value : shopRuntimeCatalog()["patches"].toArray()) {
        const auto patch = value.toObject();
        if (patch["version"].toInt(1) > version)
            continue;
        const auto site = patch["site"].toInt();
        if (site < start || site + 4 > start + owner.size())
            continue;
        if (u32(result, site - start) != rebuiltShopInstruction(patch))
            throw Error("Shop reader changed outside its rebuild");
        put32(result, site - start, quint32(patch["original"].toDouble()));
    }
    return result;
}
void verifyShopRuntime(const Rom &rom, int version) {
    rom.requireSupported();
    if (rom.revision() != 0)
        throw Error("Shop rebuilding currently supports US Rev 0");
    const auto classes = ClassPlacement::read(rom);
    for (const auto &value : shopRuntimeCatalog()["owners"].toArray()) {
        const auto owner = value.toObject();
        const auto start = owner["start"].toInt(), size = owner["size"].toInt();
        auto bytes = rom.bytes().mid(start, size);
        if (bytes.size() != size)
            throw Error("Truncated shop runtime owner");
        if (classes)
            bytes = classes->canonicalReferences(bytes, start);
        if (version) {
            bytes = ShopPlacement{0, version}.canonicalReferences(bytes, start);
            if (start == 0x19be40)
                bytes.replace(ShopPlacement::producerOffset - start, ShopPlacement::producerSize,
                              QByteArray(ShopPlacement::producerSize, 0));
        }
        const auto expected = owner[version ? "maskedSha256" : "retailSha256"].toString();
        if (sha256(bytes) != expected.toLatin1())
            throw Error("Unrecognized shop runtime at z64 " + QString::number(start, 16) +
                        "; this ROM's shop patch requires import support");
    }
}
std::optional<ShopPlacement> ShopPlacement::read(const Rom &rom) {
    const auto header = rom.bytes().mid(descriptorOffset, descriptorSize);
    if (!header.startsWith("LC05SHOP"))
        return std::nullopt;
    if (header.size() != descriptorSize || (u32(header, 8) != 1 && u32(header, 8) != 2) ||
        u32(header, 16) != dataSize || u32(header, 20) != rowCount || u32(header, 24) != rowSize ||
        header.mid(36, 24) != QByteArray(24, 0) || u32(header, 60) != crc32(header.first(60)))
        throw Error("Damaged shop placement descriptor");
    ShopPlacement result{u32(header, 12), int(u32(header, 8))};
    if (result.offset < 0x2800000 || result.offset % 16 ||
        quint64(result.offset) + dataSize > 0x4000000 ||
        quint64(result.offset) + dataSize > quint64(rom.bytes().size()) ||
        crc32(rom.bytes().mid(result.offset, dataSize)) != u32(header, 28) ||
        crc32(rom.bytes().mid(producerOffset, producerSize)) != u32(header, 32))
        throw Error("Shop data or producer differs from its declared placement");
    if (u32(rom.bytes(), producerOffset + 36) != (0x3c040000U | ((result.offset + 0x8000) >> 16)) ||
        u32(rom.bytes(), producerOffset + 40) != (0x24840000U | (result.offset & 65535)))
        throw Error("Shop producer references a different inventory allocation");
    verifyShopRuntime(rom, result.version);
    return result;
}
} // namespace lordly
