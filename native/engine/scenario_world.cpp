#include "engine/scenario_world.h"
#include "engine/scenario_map.h"
#include <QJsonArray>

namespace lordly {
namespace {
int framedCount(const QByteArray &data, int width) {
    if (data.isEmpty())
        return 0;
    const int count = u8(data, 0);
    if (data.size() < 1 + count * width)
        throw Error("Truncated scenario world records");
    return count;
}
void rowRange(int row, int count) {
    if (row < 0 || row >= count)
        throw Error("Choose an existing site or treasure");
}
QByteArray hex(const QJsonValue &value) {
    const auto text = value.toString().toLatin1();
    const auto bytes = QByteArray::fromHex(text);
    if (!value.isString() || bytes.toHex() != text.toLower())
        throw Error("World project data must contain complete hexadecimal bytes");
    return bytes;
}
} // namespace
int ScenarioWorld::declaredSiteCount(const QByteArray &data) {
    return framedCount(data, 4);
}
int ScenarioWorld::siteCount(int key) const {
    scenario(key);
    return siteCounts_.value(key);
}
int ScenarioWorld::treasureCount(const QByteArray &data) {
    return framedCount(data, 6);
}
QByteArray ScenarioWorld::treasureRecord(const QByteArray &data, int row) {
    rowRange(row, treasureCount(data));
    return data.mid(1 + row * 6, 6);
}
ScenarioWorld ScenarioWorld::load(const Rom &rom) {
    const auto resources = WorldResources::load(rom);
    ScenarioWorld result;
    const auto counts = ScenarioMap::initialSiteCounts(rom);
    result.towns_ = result.originalTowns_ = resources.towns(rom);
    if (result.towns_.size() != townCount * 28)
        throw Error("Unrecognized global site table size");
    for (const auto &member : resources.treasureMembers()) {
        const auto data = member.decode(rom);
        const int count = treasureCount(data);
        if (count > treasureCapacity)
            throw Error("Treasure resource exceeds the game's 16 scene slots");
        for (int i = 0; i < count; ++i)
            result.reservedTreasureIds_.insert(u8(data, 1 + i * 6));
    }
    for (int key = 1; key < WorldResources::keyCount; ++key) {
        WorldScenario row{resources.treasure(rom, key), resources.sites(rom, key)};
        const int count = row.sites.isEmpty() ? 0 : counts.value(key);
        declaredSiteCount(row.sites);
        if (!row.sites.isEmpty() && row.sites.size() < 1 + count * 4)
            throw Error("Scenario site resource is shorter than the game's active count");
        result.siteCounts_[key] = count;
        for (int i = 0; i < count; ++i) {
            const int id = u16(row.sites, 1 + i * 4);
            if (id < 1 || id > townCount)
                throw Error("Scenario site references a missing global record");
        }
        result.rows_[key] = row;
        result.originalTreasureGroups_[key] = resources.treasureSelector(key);
    }
    result.originals_ = result.rows_;
    return result;
}
const WorldScenario &ScenarioWorld::scenario(int key) const {
    const auto it = rows_.constFind(key);
    if (it == rows_.end())
        throw Error("Choose an existing world scenario");
    return it.value();
}
const WorldScenario &ScenarioWorld::original(int key) const {
    scenario(key);
    return originals_.constFind(key).value();
}
QByteArray ScenarioWorld::town(int id) const {
    if (id < 1 || id > townCount)
        throw Error("Site IDs are one-based and must name an existing record");
    return towns_.mid((id - 1) * 28, 28);
}
QString ScenarioWorld::townName(int id) const {
    auto name = town(id).mid(1, 20);
    if (name.contains(char(0)))
        name.truncate(name.indexOf(char(0)));
    return QString::fromLatin1(name);
}
void ScenarioWorld::validateTown(int id, const QByteArray &record) const {
    town(id);
    const auto base = originalTowns_.mid((id - 1) * 28, 28);
    if (record.size() != 28 || record[0] != base[0] || record[21] != base[21])
        throw Error("Site records must preserve their mission marker and padding");
    if (record.mid(1, 20) != base.mid(1, 20)) {
        bool ended = false;
        for (const auto ch : record.mid(1, 20)) {
            const auto c = quint8(ch);
            if (!c)
                ended = true;
            else if (ended || c < 32 || c > 126)
                throw Error(
                    "Site names use at most 20 printable ASCII characters and zero padding");
        }
    }
    if (record[24] != base[24] && u8(record, 24) > 127 && u8(record, 24) != 255)
        throw Error("Site morale is 0..127, or the exact 255 objective marker");
    if ((u8(record, 25) & 0xf0) != (u8(base, 25) & 0xf0))
        throw Error("Preserve the unclassified site capability bits");
    const int type = u8(record, 26);
    if (record[26] != base[26] && type != 9 && type != 0x29 && type != 0x49 && type != 0x89)
        throw Error("Choose town, fort, boss fort or castle");

}
void ScenarioWorld::setTown(int id, const QByteArray &record) {
    validateTown(id, record);
    towns_.replace((id - 1) * 28, 28, record);
}
void ScenarioWorld::validateScenario(int key, const WorldScenario &value) const {
    const auto &base = original(key);
    const int sites = siteCount(key);
    if (value.sites.size() != base.sites.size() || value.sites.left(1) != base.sites.left(1))
        throw Error(
            "Site membership/count is shared with mission geometry and cannot be resized here");
    if (value.sites.mid(1 + sites * 4) != base.sites.mid(1 + sites * 4))
        throw Error("Preserve inactive site descriptor data");
    for (int i = 0; i < sites; ++i) {
        const int at = 1 + i * 4, addend = u16(value.sites, at + 2);
        if (u16(value.sites, at) != u16(base.sites, at))
            throw Error("Site membership IDs must remain aligned with mission geometry");
        if (addend != u16(base.sites, at + 2) && addend != 0 && addend != 4 && addend != 0x2012)
            throw Error("New site allegiance must use a supported neutral, allied or enemy value");
    }
    const int count = treasureCount(value.treasures), oldCount = treasureCount(base.treasures);
    if (value.treasures.isEmpty() || count > treasureCapacity ||
        value.treasures.mid(1 + count * 6) != base.treasures.mid(1 + oldCount * 6))
        throw Error("Treasure lists support 16 scene slots and must preserve trailing data");
    QSet<int> ids;
    for (int i = 0; i < count; ++i) {
        const auto record = treasureRecord(value.treasures, i);
        bool unchanged = false;
        for (int j = 0; j < oldCount; ++j)
            unchanged |= record == treasureRecord(base.treasures, j);
        const int id = u8(record, 0), table = u8(record, 3), item = u16(record, 4);
        if (!unchanged && (!id || ids.contains(id) || (table != 1 && table != 2) ||
                           item > (table == 1 ? 510 : 44)))
            throw Error("Treasure needs a distinct flag ID, valid table and valid item");
        ids.insert(id);
    }
}
void ScenarioWorld::setSiteAllegiance(int key, int row, int allegiance) {
    auto next = scenario(key);
    rowRange(row, siteCount(key));
    if (allegiance < 0 || allegiance > 2)
        throw Error("Choose neutral, allied or enemy allegiance");
    put16(next.sites, 3 + row * 4, allegiance == 1 ? 0x2012 : allegiance == 2 ? 4 : 0);
    validateScenario(key, next);
    rows_[key] = next;
}
void ScenarioWorld::setTreasure(int key, int row, const QByteArray &record) {
    auto next = scenario(key);
    const auto prior = treasureRecord(next.treasures, row);
    if (record.size() != 6 || record[0] != prior[0])
        throw Error("Preserve the treasure's collection flag identity when editing");
    next.treasures.replace(1 + row * 6, 6, record);
    validateScenario(key, next);
    rows_[key] = next;
}
int ScenarioWorld::addTreasure(int key) {
    auto next = scenario(key);
    const int count = treasureCount(next.treasures);
    if (count >= treasureCapacity)
        throw Error("This scenario already uses all 16 treasure slots");
    auto used = reservedTreasureIds_;
    for (const auto &s : rows_)
        for (int i = 0; i < treasureCount(s.treasures); ++i)
            used.insert(u8(s.treasures, 1 + i * 6));
    int id = 1;
    while (id <= 255 && used.contains(id))
        ++id;
    if (id > 255)
        throw Error("All 255 nonzero collection flags are reserved");
    QByteArray record(6, 0);
    put8(record, 0, quint8(id));
    put8(record, 1, 128);
    put8(record, 2, 128);
    put8(record, 3, 2);
    put16(record, 4, 1);
    next.treasures.insert(1 + count * 6, record);
    put8(next.treasures, 0, quint8(count + 1));
    validateScenario(key, next);
    rows_[key] = next;
    return count;
}
void ScenarioWorld::removeTreasure(int key, int row) {
    auto next = scenario(key);
    const int count = treasureCount(next.treasures);
    rowRange(row, count);
    next.treasures.remove(1 + row * 6, 6);
    put8(next.treasures, 0, quint8(count - 1));
    validateScenario(key, next);
    rows_[key] = next;
}
void ScenarioWorld::restoreScenario(int key) {
    rows_[key] = original(key);
}
void ScenarioWorld::restoreTown(int id) {
    town(id);
    towns_.replace((id - 1) * 28, 28, originalTowns_.mid((id - 1) * 28, 28));
}
QJsonObject ScenarioWorld::json() const {
    QJsonObject rows;
    for (auto it = rows_.begin(); it != rows_.end(); ++it)
        rows[QString::number(it.key())] =
            QJsonObject{{"treasures", QString::fromLatin1(it->treasures.toHex())},
                        {"sites", QString::fromLatin1(it->sites.toHex())}};
    return {{"version", 1}, {"towns", QString::fromLatin1(towns_.toHex())}, {"scenarios", rows}};
}
void ScenarioWorld::apply(const QJsonObject &value) {
    if (value["version"].toInt() != 1 || !value["scenarios"].isObject() || value.size() != 3)
        throw Error("Unsupported scenario world project format");
    auto next = *this;
    const auto towns = hex(value["towns"]);
    if (towns.size() != townCount * 28)
        throw Error("Truncated global site project records");
    for (int id = 1; id <= townCount; ++id)
        next.setTown(id, towns.mid((id - 1) * 28, 28));
    const auto rows = value["scenarios"].toObject();
    if (rows.size() != rows_.size())
        throw Error("World project must retain every scenario");
    for (auto it = rows.begin(); it != rows.end(); ++it) {
        bool ok = false;
        const int key = it.key().toInt(&ok);
        const auto row = it.value().toObject();
        if (!ok || it.key() != QString::number(key) || row.size() != 2)
            throw Error("Invalid world scenario record");
        WorldScenario s{hex(row["treasures"]), hex(row["sites"])};
        next.validateScenario(key, s);
        next.rows_[key] = s;
    }
    QMap<int, int> addedIds;
    for (auto it = next.rows_.begin(); it != next.rows_.end(); ++it) {
        QSet<int> originalIds;
        const auto &base = original(it.key()).treasures;
        for (int i = 0; i < treasureCount(base); ++i)
            originalIds.insert(u8(base, 1 + i * 6));
        for (int i = 0; i < treasureCount(it->treasures); ++i) {
            const int id = u8(it->treasures, 1 + i * 6);
            if (!originalIds.contains(id)) {
                // Imported archive aliases retain a shared collection flag even
                // when their items or coordinates are edited independently later.
                const int claim = originalTreasureGroups_[it.key()];
                if (reservedTreasureIds_.contains(id) ||
                    (addedIds.contains(id) && addedIds[id] != claim))
                    throw Error(
                        "Added treasures must not reuse another treasure's collection flag");
                addedIds[id] = claim;
            }
        }
    }
    *this = next;
}
WorldPlan ScenarioWorld::plan() const {
    WorldPlan result{towns_, {}, {}};
    for (auto it = rows_.begin(); it != rows_.end(); ++it) {
        if (it->treasures != original(it.key()).treasures)
            result.treasures[it.key()] = it->treasures;
        if (it->sites != original(it.key()).sites)
            result.sites[it.key()] = it->sites;
    }
    return result;
}
bool ScenarioWorld::modified() const {
    return towns_ != originalTowns_ || rows_ != originals_;
}
int ScenarioWorld::changedEntries() const {
    int count = 0;
    for (int id = 1; id <= townCount; ++id)
        count += town(id) != originalTowns_.mid((id - 1) * 28, 28);
    for (auto it = rows_.begin(); it != rows_.end(); ++it)
        count += it.value() != original(it.key());
    return count;
}
} // namespace lordly
