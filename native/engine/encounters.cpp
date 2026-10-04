#include "engine/encounters.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <cmath>
namespace lordly {
static int number(const QJsonValue &value, int maximum) {
    const auto n = value.toDouble(-1);
    if (!value.isDouble() || !std::isfinite(n) || n != std::floor(n) || n < 0 || n > maximum)
        throw Error("Encounter value is outside its supported range");
    return int(n);
}
static int index(const QString &key, int maximum) {
    bool ok = false;
    const int id = key.toInt(&ok);
    if (!ok || id < 0 || id > maximum || QString::number(id) != key)
        throw Error("Invalid encounter record ID");
    return id;
}
void Encounters::verifyReader(const Rom &rom) {
    EncounterRate::load(rom);
}
QStringList Encounters::terrainNames() {
    return {"Plains / Roads", "Plains",       "Barrens",       "Forests",       "Marshes",
            "Highlands",      "Snowy Plains", "Snowy Barrens", "Snowy Forests", "Snowy Highlands"};
}
Encounters Encounters::load(const Rom &rom, int classCount) {
    Encounters result;
    result.globalRate_ = EncounterRate::load(rom);
    result.rewards_ = CreatureRewards::load(rom);
    result.pools_ = rom.bytes().mid(poolOffset, poolCount * slotCount * 2);
    result.rates_ = rom.bytes().mid(rateOffset, terrainCount);
    result.slots_ = rom.bytes().mid(slotOffset, terrainCount);
    // Validate every reference without changing any source data.
    result.apply(result.json(), classCount);
    result.originalPools_ = result.pools_;
    result.originalRates_ = result.rates_;
    result.originalSlots_ = result.slots_;
    result.keys_.resize(poolCount + 1);
    for (int key = 1; key <= 66; ++key) {
        const int pool = u8(rom.bytes(), 0x13c558 + key * 12) - 3;
        if (pool >= 1 && pool <= poolCount)
            result.keys_[pool].push_back(key);
    }
    QFile labels(":/game/encounters/labels.json");
    if (labels.open(QIODevice::ReadOnly)) {
        const auto rows = QJsonDocument::fromJson(labels.readAll()).object()["labels"].toObject();
        for (auto it = rows.begin(); it != rows.end(); ++it)
            result.names_[it.key().toInt()] = it.value().toString();
    }
    return result;
}
int Encounters::pairOffset(int pool, int slot) {
    if (pool < 1 || pool > poolCount || slot < 0 || slot >= slotCount)
        throw Error("Encounter pool or terrain slot is outside the table");
    return (pool - 1) * slotCount * 2 + slot * 2;
}
void Encounters::terrainIndex(int terrain) {
    if (terrain < 0 || terrain >= terrainCount)
        throw Error("Terrain ID is outside the encounter table");
}
int Encounters::classAt(int pool, int slot, int side) const {
    if (side < 0 || side > 1)
        throw Error("Encounter pair side must be 0 or 1");
    return u8(pools_, pairOffset(pool, slot) + side);
}
int Encounters::rate(int terrain) const {
    terrainIndex(terrain);
    return u8(rates_, terrain);
}
int Encounters::lookup(int terrain) const {
    terrainIndex(terrain);
    return u8(slots_, terrain);
}
QVector<int> Encounters::scenarioKeys(int pool) const {
    pairOffset(pool, 0);
    return keys_.value(pool);
}
QString Encounters::label(int pool) const {
    QStringList names;
    for (const auto key : scenarioKeys(pool)) {
        const auto name = names_.value(key, QString("Scenario key %1").arg(key));
        if (!names.contains(name))
            names << name;
    }
    return QString("Pool %1%2")
        .arg(pool)
        .arg(names.isEmpty() ? QString() : " · " + names.join(" / "));
}
void Encounters::setPair(int pool, int slot, int first, int second, int classCount) {
    const auto at = pairOffset(pool, slot);
    if (classCount < 1 || classCount > 256 || first < 0 || first >= classCount || second < 0 ||
        second >= classCount)
        throw Error("Encounter pair must reference existing class IDs or zero for no encounter");
    put8(pools_, at, quint8(first));
    put8(pools_, at + 1, quint8(second));
}
void Encounters::setTerrain(int terrain, int value, int slot) {
    terrainIndex(terrain);
    if (value < 0 || value > 100 || slot < 0 || slot > slotCount)
        throw Error("Terrain chance must be 0–100 and its pool slot must be disabled or 1–10");
    if (slot == 0 && value != 0)
        throw Error("A disabled terrain slot must have zero chance; the original selector does "
                    "not guard a zero slot after a successful rate roll");
    put8(rates_, terrain, quint8(value));
    put8(slots_, terrain, quint8(slot));
}
bool Encounters::modified() const {
    return pools_ != originalPools_ || rates_ != originalRates_ || slots_ != originalSlots_ ||
           globalRate_.modified() || rewards_.modified();
}
int Encounters::changedRows() const {
    int count = (globalRate_.modified() ? 1 : 0) + rewards_.changedRows();
    for (int p = 0; p < poolCount; ++p)
        if (pools_.mid(p * 20, 20) != originalPools_.mid(p * 20, 20))
            ++count;
    for (int t = 0; t < terrainCount; ++t)
        if (rates_[t] != originalRates_[t] || slots_[t] != originalSlots_[t])
            ++count;
    return count;
}
QJsonObject Encounters::json() const {
    QJsonArray pools, rates, slotValues;
    for (int p = 0; p < poolCount; ++p) {
        QJsonArray row;
        for (int b = 0; b < 20; ++b)
            row << u8(pools_, p * 20 + b);
        pools << row;
    }
    for (int t = 0; t < terrainCount; ++t) {
        rates << rate(t);
        slotValues << lookup(t);
    }
    return {{"version", 1},
            {"pools", pools},
            {"terrainRates", rates},
            {"terrainSlots", slotValues},
            {"globalRate", globalRate_.json()},
            {"rewards", rewards_.json()}};
}
void Encounters::apply(const QJsonObject &object, int classCount) {
    if (object["version"] != 1 || !object["pools"].isArray() ||
        object["pools"].toArray().size() != poolCount || !object["terrainRates"].isArray() ||
        object["terrainRates"].toArray().size() != terrainCount ||
        !object["terrainSlots"].isArray() ||
        object["terrainSlots"].toArray().size() != terrainCount)
        throw Error("Encounter project requires 45 pools and 32 terrain entries");
    auto next = *this;
    if (object.contains("rewards")) {
        if (!object["rewards"].isObject())
            throw Error("Invalid saved creature rewards");
        next.rewards_.apply(object["rewards"].toObject());
    }
    if (object.contains("globalRate")) {
        if (!object["globalRate"].isObject())
            throw Error("Invalid saved global encounter rate");
        next.globalRate_.apply(object["globalRate"].toObject());
    }
    const auto pools = object["pools"].toArray(), rates = object["terrainRates"].toArray(),
               slotValues = object["terrainSlots"].toArray();
    for (int p = 0; p < poolCount; ++p) {
        if (!pools[p].isArray() || pools[p].toArray().size() != 20)
            throw Error("Each encounter pool requires ten class pairs");
        const auto row = pools[p].toArray();
        for (int s = 0; s < slotCount; ++s)
            next.setPair(p + 1, s, number(row[s * 2], 255), number(row[s * 2 + 1], 255),
                         classCount);
    }
    for (int t = 0; t < terrainCount; ++t)
        next.setTerrain(t, number(rates[t], 100), number(slotValues[t], slotCount));
    *this = std::move(next);
}
void Encounters::importLegacy(const QJsonObject &object, int classCount) {
    auto next = *this;
    for (const auto &name : {"scenario_slices", "terrain_rates"})
        if (object.contains(name) && !object[name].isObject())
            throw Error("Invalid imported encounter table");
    const auto pools = object["scenario_slices"].toObject(),
               rates = object["terrain_rates"].toObject();
    for (auto it = pools.begin(); it != pools.end(); ++it) {
        const auto pool = index(it.key(), poolCount);
        pairOffset(pool, 0);
        if (!it.value().isObject())
            throw Error("Invalid imported encounter pool");
        const auto row = it.value().toObject();
        if (!row["slots"].isObject() ||
            (row.contains("row") && number(row["row"], poolCount - 1) != pool - 1))
            throw Error("Imported encounter pool framing disagrees with its ID");
        const auto slotValues = row["slots"].toObject();
        for (auto slot = slotValues.begin(); slot != slotValues.end(); ++slot) {
            const auto s = index(slot.key(), slotCount - 1);
            if (!slot.value().isArray() || slot.value().toArray().size() != 2)
                throw Error("Imported encounter pair must contain two class IDs");
            const auto pair = slot.value().toArray();
            next.setPair(pool, s, number(pair[0], 255), number(pair[1], 255), classCount);
        }
    }
    for (auto it = rates.begin(); it != rates.end(); ++it) {
        const auto terrain = index(it.key(), terrainCount - 1);
        if (!it.value().isObject())
            throw Error("Invalid imported terrain rate");
        const auto row = it.value().toObject();
        for (auto field = row.begin(); field != row.end(); ++field)
            if (field.key() != "rate" && field.key() != "rawLookup")
                throw Error("Unknown imported terrain field");
        next.setTerrain(
            terrain, row.contains("rate") ? number(row["rate"], 100) : next.rate(terrain),
            row.contains("rawLookup") ? number(row["rawLookup"], slotCount) : next.lookup(terrain));
    }
    *this = std::move(next);
}
} // namespace lordly
