#include "engine/creature_rewards.h"
#include "engine/promotion.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <cmath>
namespace lordly {
static int integer(const QJsonValue &v, int maximum = 65535) {
    const auto n = v.toDouble(-1);
    if (!v.isDouble() || !std::isfinite(n) || n != std::floor(n) || n < 0 || n > maximum)
        throw Error("Reward values must be whole numbers in their supported range");
    return int(n);
}
static void validate(const CreatureReward &row) {
    int total = 0;
    for (int i = 0; i < 3; ++i) {
        const auto raw = row.outcomes[i], id = raw & 0x7fff;
        if (raw < 0 || raw > 65535 || (raw && (id == 0 || id > ((raw & 0x8000) ? 277 : 44))))
            throw Error("Reward must be None, equipment 1–277, or consumable 1–44");
        if (row.weights[i] < 0 || row.weights[i] > 65535)
            throw Error("Reward weights must be 0–65535");
        total += row.weights[i];
    }
    if (!total)
        throw Error("At least one reward outcome must have a positive weight");
}
QJsonObject CreatureReward::json() const {
    QJsonArray items, chances;
    for (int i = 0; i < 3; ++i) {
        items << outcomes[i];
        chances << weights[i];
    }
    return {{"slots", items}, {"weights", chances}};
}
CreatureReward CreatureReward::parse(const QJsonObject &object) {
    const auto outcomes = object["slots"].toArray(), weights = object["weights"].toArray();
    if (object.size() != 2 || outcomes.size() != 3 || weights.size() != 3)
        throw Error("Each class reward requires three outcomes and three weights");
    CreatureReward result;
    for (int i = 0; i < 3; ++i) {
        result.outcomes[i] = integer(outcomes[i]);
        result.weights[i] = integer(weights[i]);
    }
    validate(result);
    return result;
}
QJsonObject CreatureRewards::catalog() {
    QFile file(":/game/encounters/rewards.json");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing creature reward source catalog");
    return QJsonDocument::fromJson(file.readAll()).object();
}
QMap<QString, quint32> CreatureRewards::services() {
    QMap<QString, quint32> result;
    const auto calls = catalog()["calls"].toObject();
    for (auto it = calls.begin(); it != calls.end(); ++it)
        result[it.key()] = quint32(it.value().toDouble());
    return result;
}
std::optional<quint32> CreatureRewards::placement(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision() != 0)
        throw Error("Creature reward rebuilding currently supports US Rev0");
    const auto spec = catalog();
    for (const auto &key : {"dma", "overlay"}) {
        const auto part = spec[key].toObject();
        if (sha256(rom.bytes().mid(part["start"].toInt(), part["size"].toInt())) !=
            part["sha256"].toString().toLatin1())
            throw Error("Creature reward runtime dependency differs from its supported source");
    }
    auto code = rom.bytes().mid(ownerOffset, ownerSize);
    const auto header = rom.bytes().mid(descriptorOffset, descriptorSize);
    if (!header.startsWith("LC05REWD")) {
        if (sha256(code) != spec["retailSha256"].toString().toLatin1())
            throw Error("Creature reward selection has an unsupported runtime modification");
        return std::nullopt;
    }
    if (header.size() != descriptorSize || u32(header, 8) != 1 || u32(header, 16) != dataSize ||
        u32(header, 20) != count || u32(header, 24) != rowSize ||
        header.mid(36, 24) != QByteArray(24, 0) || u32(header, 60) != crc32(header.first(60)))
        throw Error("Damaged creature reward descriptor");
    const auto offset = u32(header, 12);
    if (offset < 0x2800000 || offset % 16 || quint64(offset) + dataSize > 0x4000000 ||
        quint64(offset) + dataSize > quint64(rom.bytes().size()) ||
        crc32(rom.bytes().mid(offset, dataSize)) != u32(header, 28) ||
        crc32(code) != u32(header, 32))
        throw Error("Creature reward data or source differs from its declared allocation");
    if (u32(code, 0x10245c - ownerOffset) != (0x3c040000U | (offset >> 16)) ||
        u32(code, 0x102460 - ownerOffset) != (0x34840000U | (offset & 65535)))
        throw Error("Creature reward selector references a different allocation");
    put16(code, 0x10245e - ownerOffset, 0);
    put16(code, 0x102462 - ownerOffset, 0);
    if (sha256(code) != spec["nativeMaskedSha256"].toString().toLatin1())
        throw Error("Creature reward source changed outside its rebuild");
    return offset;
}
std::array<CreatureReward, CreatureRewards::count>
CreatureRewards::decodeRetail(const QByteArray &table,
                              const QMap<int, std::array<int, 3>> &weights) {
    if (table.size() != tableSize)
        throw Error("Truncated creature reward table");
    int sentinel = 0;
    while (sentinel < tableSize / 8 && u16(table, sentinel * 8) != 0)
        ++sentinel;
    if (sentinel == tableSize / 8)
        throw Error("Creature reward table has no terminating class word");
    auto read = [&](int row) {
        CreatureReward value;
        for (int s = 0; s < 3; ++s)
            value.outcomes[s] = u16(table, row * 8 + 2 + s * 2);
        if (weights.contains(row))
            value.weights = weights[row];
        validate(value);
        return value;
    };
    std::array<CreatureReward, count> result;
    result.fill(read(sentinel)); // Unlisted classes select the sentinel's outcomes.
    for (int id = 0; id < count; ++id)
        for (int row = 0; row < sentinel; ++row)
            if (u16(table, row * 8) == id) {
                result[id] = read(row);
                break;
            }
    return result;
}
CreatureRewards CreatureRewards::load(const Rom &rom) {
    const auto offset = placement(rom);
    CreatureRewards result;
    result.retail_ = rom.bytes().mid(tableOffset, tableSize);
    if (offset) {
        for (int id = 0; id < count; ++id) {
            auto &row = result.rows_[id];
            const auto at = *offset + id * rowSize;
            for (int s = 0; s < 3; ++s) {
                row.outcomes[s] = u16(rom.bytes(), at + s * 2);
                row.weights[s] = u16(rom.bytes(), at + 6 + s * 2);
            }
            if (u32(rom.bytes(), at + 12))
                throw Error("Unsupported creature reward row extension");
            validate(row);
        }
    } else
        result.rows_ = decodeRetail(result.retail_);
    result.original_ = result.rows_;
    return result;
}
const CreatureReward &CreatureRewards::row(int id) const {
    if (id < 0 || id >= count)
        throw Error("Creature reward class ID must be 0–255");
    return rows_[id];
}
void CreatureRewards::setRow(int id, const CreatureReward &value) {
    row(id);
    validate(value);
    rows_[id] = value;
}
QJsonObject CreatureRewards::json() const {
    QJsonArray rows;
    for (const auto &row : rows_)
        rows << row.json();
    return {{"version", 1}, {"rows", rows}};
}
void CreatureRewards::apply(const QJsonObject &object) {
    const auto rows = object["rows"].toArray();
    if (object.size() != 2 || object["version"] != 1 || rows.size() != count)
        throw Error("Creature rewards require all 256 byte-sized class entries");
    auto next = rows_;
    for (int i = 0; i < count; ++i)
        next[i] = CreatureReward::parse(rows[i].toObject());
    rows_ = std::move(next);
}
void CreatureRewards::importLegacy(const QJsonObject &object) {
    auto table = retail_;
    QMap<int, std::array<int, 3>> weights;
    for (auto it = object.begin(); it != object.end(); ++it) {
        bool ok = false;
        const auto row = it.key().toInt(&ok);
        if (!ok || QString::number(row) != it.key() || row < 0 || row >= tableSize / 8 ||
            !it.value().isObject())
            throw Error("Invalid browser creature reward record");
        const auto entry = it.value().toObject();
        for (auto f = entry.begin(); f != entry.end(); ++f)
            if (!QStringList{"record_index", "padByte", "classId", "slots", "weights",
                             "weights_modified"}
                     .contains(f.key()))
                throw Error("Unsupported browser creature reward field");
        if (entry.contains("record_index") && integer(entry["record_index"], 36) != row)
            throw Error("Browser creature reward index disagrees with its record");
        for (const auto &[key, offset] : {std::pair{"padByte", 0}, std::pair{"classId", 1}})
            if (entry.contains(key))
                put8(table, row * 8 + offset, quint8(integer(entry[key], 255)));
        CreatureReward value;
        for (int s = 0; s < 3; ++s)
            value.outcomes[s] = u16(table, row * 8 + 2 + s * 2);
        auto payload = value.json();
        if (entry.contains("slots"))
            payload["slots"] = entry["slots"];
        if (entry.contains("weights"))
            payload["weights"] = entry["weights"];
        if (entry.contains("weights_modified") && !entry["weights_modified"].isBool())
            throw Error("Invalid browser reward weight flag");
        value = CreatureReward::parse(payload);
        for (int s = 0; s < 3; ++s)
            put16(table, row * 8 + 2 + s * 2, quint16(value.outcomes[s]));
        weights[row] = value.weights;
    }
    const auto decoded = decodeRetail(table, weights);
    auto next = rows_;
    auto matchedRow = [](const QByteArray &bytes, int id) {
        int row = 0;
        while (row < tableSize / 8 && u16(bytes, row * 8) && u16(bytes, row * 8) != id)
            ++row;
        if (row == tableSize / 8)
            throw Error("Creature reward table has no terminating class word");
        return row;
    };
    // A browser patch is partial. Preserve other native class rewards, including
    // added classes that do not occur in the retained retail table.
    for (int id = 0; id < count; ++id) {
        const auto before = matchedRow(retail_, id), after = matchedRow(table, id);
        if (before != after || weights.contains(before) || weights.contains(after))
            next[id] = decoded[id];
    }
    rows_ = std::move(next);
}
QByteArray CreatureRewards::bytes() const {
    QByteArray result(dataSize, 0);
    for (int id = 0; id < count; ++id)
        for (int s = 0; s < 3; ++s) {
            put16(result, id * rowSize + s * 2, quint16(rows_[id].outcomes[s]));
            put16(result, id * rowSize + 6 + s * 2, quint16(rows_[id].weights[s]));
        }
    return result;
}
int CreatureRewards::changedRows() const {
    int changed = 0;
    for (int id = 0; id < CreatureRewards::count; ++id)
        if (rows_[id] != original_[id])
            ++changed;
    return changed;
}
QByteArray CreatureRewards::definitions(quint32 offset, bool native) {
    return QString(".equ LC_NATIVE_REWARDS,%1\n.equ LC_REWARDS_HI,%2\n.equ LC_REWARDS_LO,%3\n")
        .arg(native ? 1 : 0)
        .arg(offset >> 16)
        .arg(offset & 65535)
        .toLatin1();
}
} // namespace lordly
