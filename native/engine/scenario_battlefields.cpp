#include "engine/scenario_battlefields.h"
#include "engine/promotion.h"
#include "engine/mission_tables.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <optional>

namespace lordly {
namespace {
constexpr qsizetype headerSize = 64, firstAllocation = 0x2800000;
constexpr qsizetype maximumPayload = 32 * 1024 * 1024;
const QByteArray signature("LC08MSSN", 8);
struct Allocation {
    qsizetype offset = 0, size = 0;
    QJsonArray entries;
};
std::optional<Allocation> allocation(const Rom &rom) {
    const auto &bytes = rom.bytes();
    std::optional<Allocation> found;
    qsizetype cursor = firstAllocation;
    while ((cursor = bytes.indexOf(signature, cursor)) >= 0) {
        const auto at = cursor;
        cursor += signature.size();
        if (at % 16 || at + headerSize > bytes.size() || u32(bytes, at + 12) != at)
            continue;
        if (found) throw Error("Multiple custom scenario metadata allocations");
        const auto header = bytes.mid(at, headerSize);
        const auto size = u32(header, 16), packedSize = u32(header, 20), decodedSize = u32(header, 24);
        if (u32(header, 8) != 1 || size < headerSize || size % 16 ||
            quint64(at) + size > quint64(bytes.size()) || packedSize > maximumPayload ||
            decodedSize > maximumPayload || packedSize > size - headerSize ||
            u32(header, 60) != crc32(header.first(60)))
            throw Error("Custom scenario metadata allocation is damaged");
        const auto packed = bytes.mid(at + headerSize, packedSize);
        if (sha256(packed) != header.mid(28, 32).toHex())
            throw Error("Custom scenario metadata differs from its recorded snapshot");
        if (packed.size() < 4 || u32(packed, 0) != decodedSize)
            throw Error("Custom scenario metadata has an invalid compressed size");
        const auto decoded = qUncompress(packed);
        if (decoded.size() != decodedSize)
            throw Error("Custom scenario metadata could not be decompressed");
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(decoded, &error);
        if (error.error != QJsonParseError::NoError || !document.isArray())
            throw Error("Custom scenario metadata is invalid");
        validateScenarioBattlefieldMetadata(document.array());
        if (bytes.mid(at + headerSize + packedSize, size - headerSize - packedSize) !=
            QByteArray(size - headerSize - packedSize, char(-1)))
            throw Error("Custom scenario metadata has unowned trailing bytes");
        found = Allocation{at, size, document.array()};
    }
    return found;
}
} // namespace
void validateScenarioBattlefieldMetadata(const QJsonArray &entries) {
    if (entries.size() > 4 + MissionTables::keyCount - MissionTables::firstKey) throw Error("Too many custom battlefield snapshots");
    QSet<int> targets;
    for (const auto &value : entries) {
        if (!value.isObject()) throw Error("Invalid custom battlefield snapshot");
        const auto row = value.toObject();
        const int key = row["key"].toInt(-1), source = row["source"].toInt(-1);
        const auto mode = row["mode"].toString(), name = row["name"].toString().trimmed();
        if (row.size() != 5 || row["key"].toDouble() != key ||
            !MissionTables::authoringKey(key) || targets.contains(key) ||
            row["source"].toDouble() != source || source < 1 || source > 64 || source == 54 ||
            source == key || (mode != "exact" && mode != "mapOnly" && mode != "blank") ||
            name.isEmpty() || name.size() > 120 || !row["snapshot"].isObject())
            throw Error("Invalid custom battlefield identity or snapshot");
        const auto snapshot = row["snapshot"].toObject();
        if (snapshot.size() != 2 || !snapshot["art"].isObject() || !snapshot["units"].isObject())
            throw Error("Custom battlefield needs its artwork and scenario snapshots");
        const auto units = snapshot["units"].toObject();
        if (units["source"].toInt(-1) != source || units["mode"].toString() != mode)
            throw Error("Custom battlefield identity differs from its captured scenario");
        targets.insert(key);
    }
}
QJsonArray readScenarioBattlefields(const Rom &rom) {
    const auto record = allocation(rom);
    return record ? record->entries : QJsonArray{};
}
QByteArray writeScenarioBattlefields(const Rom &rom, const QJsonArray &entries) {
    validateScenarioBattlefieldMetadata(entries);
    const auto prior = allocation(rom);
    if ((!prior && entries.isEmpty()) || (prior && prior->entries == entries)) return rom.bytes();
    auto bytes = rom.bytes();
    if (entries.isEmpty()) {
        if (prior->offset + prior->size == bytes.size()) bytes.resize(prior->offset);
        else bytes.replace(prior->offset, prior->size, QByteArray(prior->size, char(-1)));
        return bytes;
    }
    const auto decoded = QJsonDocument(entries).toJson(QJsonDocument::Compact);
    if (decoded.size() > maximumPayload) throw Error("Custom scenario snapshots exceed their size limit");
    const auto packed = qCompress(decoded, 9);
    const auto size = (headerSize + packed.size() + 15) & ~qsizetype(15);
    const auto at = prior && prior->size >= size ? prior->offset : (bytes.size() + 15) & ~qsizetype(15);
    const auto capacity = prior && at == prior->offset ? prior->size : size;
    if (at < firstAllocation || at + capacity > 0x4000000)
        throw Error("Custom scenario snapshots exceed cartridge space");
    if (prior && at != prior->offset)
        bytes.replace(prior->offset, prior->size, QByteArray(prior->size, char(-1)));
    if (at + capacity > bytes.size()) bytes += QByteArray(at + capacity - bytes.size(), char(-1));
    QByteArray header(headerSize, 0);
    header.replace(0, 8, signature);
    put32(header, 8, 1); put32(header, 12, quint32(at)); put32(header, 16, quint32(capacity));
    put32(header, 20, quint32(packed.size())); put32(header, 24, quint32(decoded.size()));
    header.replace(28, 32, QByteArray::fromHex(sha256(packed)));
    put32(header, 60, crc32(header.first(60)));
    bytes.replace(at, capacity, header + packed + QByteArray(capacity - headerSize - packed.size(), char(-1)));
    return bytes;
}
} // namespace lordly
