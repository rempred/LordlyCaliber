#include "engine/scenarios.h"
#include "engine/squad_members.h"
#include "engine/scenario_member_runtime.h"
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
            s.squads << readSquadRecord(result.enemies_, id);
            // Preserve the effective deployment allegiance across a template remap.
            if (automaticAlly(key, id))
                row[0] = char(u8(row, 0) | 128);
        }
        result.rows_[key] = s;
    }
    bool extended = false;
    for (const auto &row : result.rows_)
        for (const auto &record : row.squads) extended |= independentSquad(record);
    if (extended && !scenarioMemberRuntimeInstalled(rom))
        throw Error("Independent squad data needs its matching member constructor");
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
    return int(squadMembers(record).size());
}
static void levels(const QByteArray &record, int base) {
    for (const auto &member : squadMembers(record)) {
        const int level = base + member.levelOffset;
        if (level < 1 || level > 99)
            throw Error("Base level and each member offset must produce level 1–99");
    }
}
static QByteArray composition(const QByteArray &record) {
    QByteArray value;
    if (!record.isEmpty())
        for (const auto &member : squadMembers(record)) {
            value += char(member.classId);
            value += char(member.cell);
        }
    return value;
}
int Scenarios::carryingCapacity(const QByteArray &record, const ClassData &classes) {
    int result = 0;
    for (const auto &member : squadMembers(record)) {
        if (member.classId < 1 || member.classId >= classes.entries().size())
            throw Error("Squad supplies refer to an unavailable class");
        result += u8(classes.entries()[member.classId].record, ClassData::itemCapacityOffset);
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
    if (e.header.mid(11, 2) != base.layout.header.mid(11, 2) &&
        (e.startHour() > 23 || e.startMinute() > 59))
        throw Error("Starting time must be between 00:00 and 23:59");
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
        const auto members = squadMembers(record);
        const auto previous = before.isEmpty() ? QVector<CustomMember>{} : squadMembers(before);
        for (const auto &member : members) {
            const int id = member.classId;
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
            validateSquadFormation(members, classes);
        bool changedLevels = members.size() != previous.size();
        for (int m = 0; m < members.size(); ++m) {
            changedLevels |= m >= previous.size() || members[m].levelOffset != previous[m].levelOffset;
            for (int side = 0; side < 2; ++side)
                if (members[m].equipment[side] > 510 &&
                    (m >= previous.size() || members[m].equipment[side] != previous[m].equipment[side]))
                    throw Error("Choose equipment 0–510; unknown original equipment can only be preserved");
        }
        if (changedComposition || changedLevels || e.baseLevel() != base.layout.baseLevel())
            levels(record, e.baseLevel());
        // Preserve the unresolved original level join while retaining ordinary edits.
        if (s.key == 25 && source == 40 && !before.isEmpty() && changedLevels)
            for (int m = 1; m < std::min(members.size(), previous.size()); ++m)
                if (members[m].levelOffset != previous[m].levelOffset)
                    throw Error("This deployment's follower level join is unresolved; its base level remains editable");
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
void Scenarios::setStartTime(int key, int hour, int minute, const ClassData &classes) {
    auto next = scenario(key);
    next.layout.setStartTime(hour, minute);
    replace(std::move(next), classes);
}
void Scenarios::setBaseLevel(int key, int level, const ClassData &classes) {
    auto next = scenario(key);
    next.layout.setBaseLevel(level);
    replace(std::move(next), classes);
}
void Scenarios::setSquad(int key, int row, const QByteArray &record, const ClassData &classes) {
    auto next = scenario(key);
    rowIndex(next, row);
    squadMembers(record);
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
    squadMembers(squad);
    next.squads[row] = squad;
    replace(std::move(next), classes);
}
int Scenarios::addSquad(int key, int templateRow, const ClassData &classes) {
    auto next = scenario(key);
    if (templateRow >= 0) rowIndex(next, templateRow);
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
    QByteArray row = templateRow >= 0 ? next.layout.deployments[templateRow] : QByteArray(18, 0);
    put8(row, 0, quint8(source));
    put16(row, 1, 0);
    put8(row, 6, 1); // Original built-in hold-position node.
    if (templateRow < 0)
        put8(row, 8, 1); // Autonomous; deployment tactics are one-based.
    row.replace(10, 8, QByteArray(8, 0));
    if (!next.layout.deployments.isEmpty())
        next.layout.deployments.last().replace(16, 2, QByteArray(2, 0));
    next.layout.deployments << row;
    if (templateRow >= 0) next.squads << next.squads[templateRow];
    else {
        QByteArray record(35, 0);
        put8(record, 0, 5); put8(record, 6, 9);
        next.squads << record;
    }
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
        if (!indices.contains(readSquadRecord(enemies_, i + 1)))
            indices[readSquadRecord(enemies_, i + 1)] = i + 1;
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
                if (id - 1 + record.size() / 35 > 65535)
                    throw Error("Enemy squad library exceeds the template ID width");
                indices[record] = id;
                result.enemies += record;
            }
            // An explicitly non-allied deployment must not accidentally select one
            // of the original key/ID pairs that the game promotes to an ally.
            if (!(u8(row, 0) & 128) && automaticAlly(s.key, id)) {
                id = int(result.enemies.size() / 35) + 1;
                if (id - 1 + record.size() / 35 > 65535)
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
