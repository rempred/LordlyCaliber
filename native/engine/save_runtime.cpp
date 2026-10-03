#include "engine/save_runtime.h"
#include "engine/promotion.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
namespace lordly {
QJsonObject saveRuntimeCatalog() {
    QFile file(":/game/classes/save-owners.json");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing save runtime owner catalog");
    return QJsonDocument::fromJson(file.readAll()).object();
}
quint32 saveRuntimeOwnerChecksum(const QByteArray &rom) {
    const auto catalog = saveRuntimeCatalog();
    auto ranges = catalog["owners"].toArray();
    ranges.append(catalog["callbacks"]);
    QByteArray data;
    for (const auto value : ranges) {
        const auto row = value.toObject();
        const auto bytes = rom.mid(row["start"].toInt(), row["size"].toInt());
        if (bytes.size() != row["size"].toInt())
            throw Error("Truncated save runtime owner");
        data += bytes;
    }
    return crc32(data);
}
void SavePlacement::validate() const {
    if ((!codeBytes && (codeChecksum || ownerChecksum)) || codeBytes > capacity || codeBytes % 4)
        throw Error("Unsupported expanded save code allocation");
}
void SavePlacement::verify(const QByteArray &rom) const {
    validate();
    if (!installed())
        return;
    if (crc32(rom.mid(romAddress, codeBytes)) != codeChecksum ||
        saveRuntimeOwnerChecksum(rom) != ownerChecksum)
        throw Error("Expanded save code or dispatch changed outside its rebuild");
    auto entry = [&](quint32 address) {
        if (address % 4 || address < ramAddress || address >= ramAddress + codeBytes)
            throw Error("Expanded save entry is outside its code allocation");
    };
    const auto catalog = saveRuntimeCatalog();
    for (const auto value : catalog["owners"].toArray()) {
        const auto at = value.toObject()["start"].toInt();
        const auto word = u32(rom, at);
        if (word >> 26 != 2 || u32(rom, at + 4))
            throw Error("Invalid expanded save entry dispatch");
        entry(0x80000000U | ((word & 0x3ffffffU) << 2));
    }
    const auto table = catalog["callbacks"].toObject()["start"].toInt();
    entry(u32(rom, table + 28 + 8));
    for (int i = 0; i < 3; ++i)
        entry(u32(rom, table + 4 * 28 + i * 4));
    if (u32(rom, table + 4 * 28 + 12) != 0x1844 ||
        rom.mid(table + 4 * 28 + 16, 12) != QByteArray(12, 0))
        throw Error("Expanded discovery callback has an invalid record allocation");
}
} // namespace lordly
