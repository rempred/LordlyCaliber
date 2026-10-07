#include "engine/scenarios.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <algorithm>

namespace lordly {
bool Scenarios::automaticAlly(int key, int id) {
    return (key == 4 && id == 517) || (key == 21 && id == 470) || (key == 27 && id == 350) ||
           (key == 42 && id == 372);
}
static void rowIndex(const Scenario &s, int row) {
    if (row < 0 || row >= s.squads.size())
        throw Error("Choose an existing scenario squad");
}
static int sourceId(const QByteArray &row) {
    return u8(row, 0) & 127;
}
static int sourceRow(const Scenario &s, int source) {
    for (int i = 0; i < s.layout.deployments.size(); ++i)
        if (sourceId(s.layout.deployments[i]) == source)
            return i;
    return -1;
}
QJsonObject Scenario::json() const {
    auto logical = layout;
    for (auto &row : logical.deployments)
        put16(row, 1, 0); // Record IDs are assigned by the export's shared library.
    QJsonArray records;
    for (const auto &record : squads)
        records << QString::fromLatin1(record.toHex());
    return {{"layout", QString::fromLatin1(logical.encode().toHex())}, {"squads", records}};
}
Scenarios Scenarios::load(const Rom &rom) {
    const auto resources = ScenarioResources::load(rom);
    Scenarios result;
    result.enemies_ = resources.enemyData();
    const auto originalClasses = ClassData::load(rom);
    for (const auto &entry : originalClasses.entries())
        result.originalClassLimits_
            << qMakePair(int(u8(entry.record, ClassData::unitSizeOffset)),
                         int(u8(entry.record, ClassData::itemCapacityOffset)));
    QFile labels(":/game/encounters/labels.json");
    QJsonObject names;
    if (labels.open(QIODevice::ReadOnly))
        names = QJsonDocument::fromJson(labels.readAll()).object()["labels"].toObject();
    for (int key = 1; key < ScenarioResources::keyCount; ++key) {
        if (key == 54)
            continue;
        Scenario s{key,
                   names[QString::number(key)].toString(QString("Scenario %1").arg(key)),
                   Eset::decode(resources.scenario(rom, key)),
                   {}};
        for (auto &row : s.layout.deployments) {
            const int id = u16(row, 1);
            if (id < 1 || id > result.enemies_.size() / 35)
                throw Error(QString("Scenario %1 references a missing squad template").arg(key));
            s.squads << result.enemies_.mid((id - 1) * 35, 35);
            // Preserve the effective deployment allegiance across a template remap.
            if (automaticAlly(key, id))
                row[0] = char(u8(row, 0) | 128);
        }
        result.rows_[key] = s;
    }
    result.originals_ = result.rows_;
    return result;
}
const Scenario &Scenarios::scenario(int key) const {
    const auto it = rows_.constFind(key);
    if (it == rows_.end())
        throw Error("Choose a supported playable scenario");
    return it.value();
}
const Scenario &Scenarios::original(int key) const {
    scenario(key);
    return originals_.constFind(key).value();
}
int Scenarios::memberCount(const QByteArray &record) {
    if (record.size() != 35)
        throw Error("A squad template must contain 35 bytes");
    int count = 1;
    for (const int at : {13, 14, 15, 22, 23, 24})
        count += u8(record, at) != 0;
    return count;
}
static int signedByte(const QByteArray &bytes, int at) {
    const int raw = u8(bytes, at);
    return raw >= 128 ? raw - 256 : raw;
}
static void levels(const QByteArray &record, int base) {
    for (const int at : {0, 7, 16}) {
        if (at && !u8(record, at + 6) && !u8(record, at + 7) && !u8(record, at + 8))
            continue;
        const int level = base + signedByte(record, at + 1);
        if (level < 1 || level > 99 || ((base + u8(record, at + 1)) & 255) != level)
            throw Error("Base level and each active cohort offset must produce level 1–99");
    }
}
static QByteArray composition(const QByteArray &record) {
    if (record.isEmpty())
        return {};
    QByteArray value;
    for (const int at : {0, 6, 7, 13, 14, 15, 16, 22, 23, 24})
        value += record[at];
    return value;
}
static void formation(const QByteArray &record, const ClassData &classes) {
    QSet<int> cells;
    QVector<QPair<int, int>> occupants;
    int cost = 0;
    for (const int at : {0, 7, 16}) {
        const int first = at == 0 ? 6 : at + 6, last = at == 0 ? 6 : at + 8;
        for (int p = first; p <= last; ++p) {
            const int cell = u8(record, p);
            if (!cell && at != 0)
                continue;
            if (cell < 1 || cell > 9 || cells.contains(cell))
                throw Error("Every squad member needs a different formation cell from 1 through 9");
            cells.insert(cell);
            const int id = u8(record, at);
            if (id < 1 || id >= classes.entries().size())
                throw Error("Squad member refers to an unavailable class");
            const int size = u8(classes.entries()[id].record, ClassData::unitSizeOffset);
            if (size > 2)
                throw Error("Squad uses a class with an unknown unit size");
            cost += size == 2 ? 2 : 1;
            occupants << qMakePair(cell - 1, size);
        }
    }
    if (occupants.size() > 5 || cost > 5)
        throw Error("Squad exceeds its five formation slots");
    for (const auto &a : occupants)
        if (a.second == 2)
            for (const auto &b : occupants)
                if (a.first != b.first && std::abs(a.first / 3 - b.first / 3) <= 1 &&
                    std::abs(a.first % 3 - b.first % 3) <= 1)
                    throw Error("A large squad member needs adjacent formation cells clear");
}
int Scenarios::carryingCapacity(const QByteArray &record, const ClassData &classes) {
    if (record.size() != 35)
        throw Error("Squad template must contain 35 bytes");
    int result = 0;
    for (const int at : {0, 7, 16}) {
        const int first = at == 0 ? 6 : at + 6, last = at == 0 ? 6 : at + 8;
        for (int p = first; p <= last; ++p) {
            if (at && !u8(record, p))
                continue;
            const int id = u8(record, at);
            if (id < 1 || id >= classes.entries().size())
                throw Error("Squad supplies refer to an unavailable class");
            result += u8(classes.entries()[id].record, ClassData::itemCapacityOffset);
        }
    }
    return result;
}
void Scenarios::validate(const Scenario &s, const ClassData &classes) const {
    const auto &base = original(s.key);
    const auto &e = s.layout;
    if (e.deployments.size() != s.squads.size() || e.deployments.size() > 20 ||
        e.nodes.size() > 16 || e.conditions.size() > 16)
        throw Error("Scenario supports up to 20 squads, 16 movement nodes and 16 conditions");
    if (e.baseLevel() < 1 || e.baseLevel() > 99)
        throw Error("Scenario base level must be 1–99");
    e.encode();
    validateRoutes(s);
    QSet<int> sources;
    int total = 0;
    for (int i = 0; i < s.squads.size(); ++i) {
        const auto &row = e.deployments[i], &record = s.squads[i];
        const int source = sourceId(row), beforeIndex = sourceRow(base, source);
        const QByteArray before = beforeIndex < 0 ? QByteArray() : base.squads[beforeIndex];
        const QByteArray beforeRow =
            beforeIndex < 0 ? QByteArray() : base.layout.deployments[beforeIndex];
        if (source > 49 || sources.contains(source) || (beforeIndex < 0 && source < 30))
            throw Error("Added squads need an unused source ID from 30 through 49");
        sources.insert(source);
        total += memberCount(record);
        // Preserve unrelated unusual source values when changing equipment or levels.
        const bool changedComposition = composition(record) != composition(before);
        bool changedSize = false, changedCapacity = false;
        for (const int at : {0, 7, 16}) {
            if (at && !u8(record, at + 6) && !u8(record, at + 7) && !u8(record, at + 8))
                continue;
            const int id = u8(record, at);
            if (id >= classes.entries().size())
                throw Error("Squad member refers to an unavailable class");
            if (id >= originalClassLimits_.size()) {
                changedSize = changedCapacity = true;
            } else {
                changedSize |= u8(classes.entries()[id].record, ClassData::unitSizeOffset) !=
                               originalClassLimits_[id].first;
                changedCapacity |=
                    u8(classes.entries()[id].record, ClassData::itemCapacityOffset) !=
                    originalClassLimits_[id].second;
            }
        }
        if (changedComposition || changedSize)
            formation(record, classes);
        if (changedComposition || before.isEmpty() || record[1] != before[1] ||
            record[8] != before[8] || record[17] != before[17] ||
            e.baseLevel() != base.layout.baseLevel())
            levels(record, e.baseLevel());
        for (const int at : {2, 4, 9, 11, 18, 20})
            if (u16(record, at) > 510 && (before.isEmpty() || u16(record, at) != u16(before, at)))
                throw Error(
                    "Choose equipment 0–510; unknown original equipment can only be preserved");
        // The accepted ambiguous join is zero-based EDAT 199: key 25, source 40.
        if (s.key == 25 && source == 40 && !before.isEmpty() &&
            (record[8] != before[8] || record[17] != before[17]))
            throw Error(
                "This deployment's B/C level join is unresolved; its base level remains editable");
        if (changedComposition || changedCapacity || beforeRow.isEmpty() ||
            row.mid(13, 3) != beforeRow.mid(13, 3)) {
            const int cap = carryingCapacity(record, classes);
            if (cap < 1 || cap > 10)
                throw Error("Squad composition must carry between one and ten supplies");
            const std::array<int, 7> healing{0, 1, 2, 3, 2, 3, 2};
            const int heal = u8(row, 13), fruit = u8(row, 14), revive = u8(row, 15);
            if (heal > 6 || fruit > 4 || revive > 2 ||
                (fruit == 4 && (beforeRow.isEmpty() || u8(beforeRow, 14) != 4)))
                throw Error("Choose a supported initial-supply preset");
            const int count = healing[heal] + ((fruit > 0 && fruit < 4) ? 2 : 0) + revive;
            if (count > cap)
                throw Error("Initial supplies exceed this squad's carrying capacity");
        }
        if (beforeRow.isEmpty() || row.mid(10, 3) != beforeRow.mid(10, 3)) {
            const int value = u16(row, 11), id = value & 0x7fff;
            if (id > ((u8(row, 10) || (value & 0x8000)) ? 510 : 44))
                throw Error("Squad drop refers to an unavailable item");
        }
    }
    if (total > 98)
        throw Error("Scenario exceeds the 98 resettable transient character records");
}
void Scenarios::replace(Scenario value, const ClassData &classes) {
    validate(value, classes);
    // Canonicalize dependent framing, including the final row's shared tail.
    value.layout = Eset::decode(value.layout.encode());
    rows_[value.key] = std::move(value);
}
void Scenarios::setBaseLevel(int key, int level, const ClassData &classes) {
    auto next = scenario(key);
    next.layout.setBaseLevel(level);
    replace(std::move(next), classes);
}
void Scenarios::setSquad(int key, int row, const QByteArray &record, const ClassData &classes) {
    auto next = scenario(key);
    rowIndex(next, row);
    if (record.size() != 35)
        throw Error("Squad template must contain 35 bytes");
    next.squads[row] = record;
    replace(std::move(next), classes);
}
void Scenarios::setDeployment(int key, int row, const QByteArray &record,
                              const ClassData &classes) {
    editSquad(key, row, scenario(key).squads.value(row), record, classes);
}
void Scenarios::editSquad(int key, int row, const QByteArray &squad, const QByteArray &record,
                          const ClassData &classes) {
    auto next = scenario(key);
    rowIndex(next, row);
    if (record.size() != 18 || sourceId(record) != sourceId(next.layout.deployments[row]) ||
        record.mid(1, 2) != next.layout.deployments[row].mid(1, 2))
        throw Error("Deployment identity and allocated template IDs are managed by the editor");
    next.layout.deployments[row] = record;
    if (squad.size() != 35)
        throw Error("Squad template must contain 35 bytes");
    next.squads[row] = squad;
    replace(std::move(next), classes);
}
int Scenarios::addSquad(int key, int templateRow, const ClassData &classes) {
    auto next = scenario(key);
    rowIndex(next, templateRow);
    if (next.squads.size() >= 20)
        throw Error("This scenario already has 20 squads");
    QSet<int> used;
    for (const auto &row : next.layout.deployments)
        used.insert(sourceId(row));
    int source = 30;
    while (source < 50 && used.contains(source))
        ++source;
    if (source == 50)
        throw Error("This scenario has no free enemy source ID");
    QByteArray row = next.layout.deployments[templateRow];
    put8(row, 0, quint8(source));
    put16(row, 1, 0);
    put8(row, 6, 1); // Original built-in hold-position node.
    row.replace(10, 8, QByteArray(8, 0));
    if (!next.layout.deployments.isEmpty())
        next.layout.deployments.last().replace(16, 2, QByteArray(2, 0));
    next.layout.deployments << row;
    next.squads << next.squads[templateRow];
    const int result = int(next.squads.size()) - 1;
    replace(std::move(next), classes);
    return result;
}
void Scenarios::removeSquad(int key, int row, const ClassData &classes) {
    auto next = scenario(key);
    rowIndex(next, row);
    const int source = sourceId(next.layout.deployments[row]);
    if (sourceRow(original(key), source) >= 0)
        throw Error(
            "Original squads may be referenced by game events; only added squads can be removed");
    for (const auto &condition : next.layout.conditions)
        if (u8(condition, 1) == 8 && u8(condition, 6) == source)
            throw Error("Change the condition watching this squad before removing it");
    // Route groups use each preceding row's final pair for the following row.
    const auto previousPair = row ? next.layout.deployments[row - 1].right(2) : next.layout.seed;
    if (previousPair != QByteArray(2, 0))
        throw Error("Remove this squad from its route group before deleting it");
    if (row > 0 && row + 1 < next.squads.size())
        next.layout.deployments[row - 1].replace(16, 2, next.layout.deployments[row].right(2));
    next.layout.deployments.removeAt(row);
    next.squads.removeAt(row);
    replace(std::move(next), classes);
}
void Scenarios::restore(int key) {
    rows_[key] = original(key);
}
QJsonObject Scenarios::json() const {
    QJsonObject scenarios;
    for (auto it = rows_.begin(); it != rows_.end(); ++it)
        scenarios[QString::number(it.key())] = it->json();
    return {{"version", 1}, {"scenarios", scenarios}};
}
static QByteArray raw(const QJsonValue &value) {
    const auto hex = value.toString().toLatin1();
    const auto bytes = QByteArray::fromHex(hex);
    if (!value.isString() || bytes.toHex() != hex.toLower())
        throw Error("Invalid saved scenario bytes");
    return bytes;
}
void Scenarios::apply(const QJsonObject &value, const ClassData &classes) {
    if (value.size() != 2 || value["version"] != 1 || !value["scenarios"].isObject())
        throw Error("Invalid saved scenarios");
    auto next = *this;
    const auto values = value["scenarios"].toObject();
    if (values.size() != rows_.size())
        throw Error("Saved scenario directory is incomplete");
    for (auto it = values.begin(); it != values.end(); ++it) {
        bool ok = false;
        const int key = it.key().toInt(&ok);
        if (!ok || QString::number(key) != it.key() || !it.value().isObject())
            throw Error("Invalid saved scenario key");
        auto row = next.scenario(key);
        const auto object = it.value().toObject();
        if (object.size() != 2 || !object["squads"].isArray())
            throw Error("Invalid saved scenario record");
        row.layout = Eset::decode(raw(object["layout"]));
        row.squads.clear();
        for (const auto &v : object["squads"].toArray())
            row.squads << raw(v);
        if (row.squads.size() != row.layout.deployments.size())
            throw Error("Saved scenario deployments and squads disagree");
        for (const auto &deployment : row.layout.deployments)
            if (u16(deployment, 1))
                throw Error("Saved scenario uses allocated template IDs instead of local squads");
        if (row.json() != next.original(key).json())
            next.replace(std::move(row), classes);
        else
            next.rows_[key] = next.original(key);
    }
    *this = std::move(next);
}
int Scenarios::changedScenarios() const {
    int count = 0;
    for (auto it = rows_.begin(); it != rows_.end(); ++it)
        count += it->json() != originals_[it.key()].json();
    return count;
}
ScenarioPlan Scenarios::plan(const ClassData &classes) const {
    ScenarioPlan result{enemies_, {}};
    QMap<QByteArray, int> indices;
    for (int i = 0; i < enemies_.size() / 35; ++i)
        if (!indices.contains(enemies_.mid(i * 35, 35)))
            indices[enemies_.mid(i * 35, 35)] = i + 1;
    validateForExport(classes);
    for (const auto &s : rows_) {
        const auto &base = original(s.key);
        if (s.json() == base.json())
            continue;
        auto layout = s.layout;
        for (int i = 0; i < s.squads.size(); ++i) {
            auto &row = layout.deployments[i];
            const auto &record = s.squads[i];
            const int before = sourceRow(base, sourceId(row));
            int id = before >= 0 && base.squads[before] == record
                         ? u16(base.layout.deployments[before], 1)
                         : indices.value(record);
            if (!id) {
                id = int(result.enemies.size() / 35) + 1;
                if (id > 65535)
                    throw Error("Enemy squad library exceeds the template ID width");
                indices[record] = id;
                result.enemies += record;
            }
            // An explicitly non-allied deployment must not accidentally select one
            // of the original key/ID pairs that the game promotes to an ally.
            if (!(u8(row, 0) & 128) && automaticAlly(s.key, id)) {
                id = int(result.enemies.size() / 35) + 1;
                if (id > 65535)
                    throw Error("Enemy squad library exceeds the template ID width");
                result.enemies += record;
            }
            put16(row, 1, quint16(id));
        }
        result.esets[s.key] = layout.encode();
    }
    return result;
}
void Scenarios::validateForExport(const ClassData &classes) const {
    bool classLimitsChanged = classes.entries().size() != originalClassLimits_.size();
    for (int id = 0; id < std::min(classes.entries().size(), originalClassLimits_.size()); ++id)
        classLimitsChanged |= u8(classes.entries()[id].record, ClassData::unitSizeOffset) !=
                                  originalClassLimits_[id].first ||
                              u8(classes.entries()[id].record, ClassData::itemCapacityOffset) !=
                                  originalClassLimits_[id].second;
    for (const auto &s : rows_) {
        if (!classLimitsChanged && s.json() == original(s.key).json())
            continue;
        try {
            validate(s, classes);
        } catch (const Error &error) {
            throw Error(QString("Scenario %1 (%2): %3")
                            .arg(s.key)
                            .arg(s.name, QString::fromUtf8(error.what())));
        }
    }
}
} // namespace lordly
