#include "engine/custom_squads.h"
#include "engine/custom_runtime.h"
#include <QJsonArray>
#include <QSet>
#include <cmath>
namespace lordly {
static int integer(const QJsonValue &v, int minimum, int maximum) {
    const auto n = v.toDouble(minimum - 1);
    if (!v.isDouble() || !std::isfinite(n) || n != std::floor(n) || n < minimum || n > maximum)
        throw Error("Custom squad value is outside its supported range");
    return int(n);
}
int CustomSquads::key(int scenario, int slot) {
    if (scenario < 0 || scenario > 255 || slot < 0 || slot > 9)
        throw Error("Custom squad requires a scenario key 0–255 and terrain slot 0–9");
    return scenario * 10 + slot;
}
QJsonObject CustomSquad::json() const {
    QJsonArray people, bonus;
    for (const auto &m : members)
        people << QJsonObject{{"classId", m.classId},
                              {"levelOffset", m.levelOffset},
                              {"cell", m.cell},
                              {"equipment", QJsonArray{m.equipment[0], m.equipment[1]}}};
    for (const auto &[id, value] : bonuses)
        bonus << QJsonObject{{"classId", id}, {"bonus", value}};
    return {{"scenarioKey", scenarioKey},
            {"terrainSlot", terrainSlot},
            {"enabled", enabled},
            {"label", label},
            {"members", people},
            {"persuasion", QJsonObject{{"mode", fixedPersuasion ? "fixed" : "original"},
                                       {"chance", chance},
                                       {"bonuses", bonus}}},
            {"retreatHp", retreatHp},
            {"rewards", rewards.json()}};
}
CustomSquad CustomSquad::parse(const QJsonObject &o, int classCount) {
    if (classCount < 165 || classCount > 256 || o.size() != 8 || !o["enabled"].isBool() ||
        !o["label"].isString() || !o["members"].isArray() || !o["persuasion"].isObject() ||
        !o["rewards"].isObject())
        throw Error("Invalid custom squad record");
    CustomSquad s;
    s.scenarioKey = integer(o["scenarioKey"], 0, 255);
    s.terrainSlot = integer(o["terrainSlot"], 0, 9);
    s.enabled = o["enabled"].toBool();
    s.label = o["label"].toString().trimmed();
    if (s.label.isEmpty() || s.label.size() > 24)
        throw Error("Encounter card text needs 1–24 characters");
    for (const auto ch : s.label)
        if (ch.unicode() < 32 || ch.unicode() > 126 || ch == '@' || ch == '"' || ch == '\\')
            throw Error(
                "Encounter card text supports printable ASCII except @, quote and backslash");
    const auto people = o["members"].toArray();
    if (people.size() < 2 || people.size() > 5)
        throw Error("Custom squads need two through five members");
    s.members.clear();
    QSet<int> cells;
    for (const auto &v : people) {
        const auto m = v.toObject();
        if (!v.isObject() || m.size() != 4 || !m["equipment"].isArray() ||
            m["equipment"].toArray().size() != 2)
            throw Error("Custom member needs class, level offset, cell and two equipment choices");
        const auto equipment = m["equipment"].toArray();
        CustomMember member{integer(m["classId"], 2, classCount - 1),
                            integer(m["levelOffset"], -128, 127),
                            integer(m["cell"], 0, 8),
                            {integer(equipment[0], 0, 510), integer(equipment[1], 0, 510)}};
        if (cells.contains(member.cell))
            throw Error("Custom squad members cannot share a formation cell");
        cells.insert(member.cell);
        s.members << member;
    }
    const auto persuade = o["persuasion"].toObject();
    const auto mode = persuade["mode"].toString();
    if (persuade.size() != 3 || !persuade["bonuses"].isArray() ||
        persuade["bonuses"].toArray().size() > 3 || (mode != "fixed" && mode != "original"))
        throw Error("Invalid custom persuasion settings");
    s.fixedPersuasion = mode == "fixed";
    s.chance = integer(persuade["chance"], 0, 100);
    QSet<int> bonusClasses;
    for (const auto &v : persuade["bonuses"].toArray()) {
        const auto b = v.toObject();
        if (!v.isObject() || b.size() != 2)
            throw Error("Invalid persuasion class bonus");
        const auto id = integer(b["classId"], 2, classCount - 1),
                   bonus = integer(b["bonus"], 1, 100);
        if (bonusClasses.contains(id))
            throw Error("Persuasion cannot repeat a player leader class bonus");
        bonusClasses.insert(id);
        s.bonuses << qMakePair(id, bonus);
    }
    s.retreatHp = integer(o["retreatHp"], 0, 100);
    s.rewards = CreatureReward::parse(o["rewards"].toObject());
    return s;
}
QByteArray CustomSquad::bytes() const {
    const auto s = parse(json(), 256);
    QByteArray b(CustomPlacement::profileBytes, 0);
    put8(b, 0, s.enabled);
    put8(b, 1, quint8(s.members.size()));
    put8(b, 2, s.fixedPersuasion);
    put8(b, 3, quint8(s.chance));
    put8(b, 4, quint8(s.retreatHp));
    put8(b, 5, quint8(s.scenarioKey));
    put8(b, 6, quint8(s.terrainSlot));
    for (int i = 0; i < s.members.size(); ++i) {
        const auto &m = s.members[i];
        const auto at = 8 + i * 8;
        put8(b, at, quint8(m.classId));
        put8(b, at + 1, quint8(m.levelOffset & 255));
        put8(b, at + 2, quint8(m.cell));
        put16(b, at + 4, quint16(m.equipment[0]));
        put16(b, at + 6, quint16(m.equipment[1]));
    }
    for (int i = 0; i < s.bonuses.size(); ++i) {
        put8(b, 48 + i * 2, quint8(s.bonuses[i].first));
        put8(b, 49 + i * 2, quint8(s.bonuses[i].second));
    }
    for (int i = 0; i < 3; ++i) {
        put16(b, 56 + i * 4, quint16(s.rewards.outcomes[i]));
        put16(b, 58 + i * 4, quint16(s.rewards.weights[i]));
    }
    const auto message = QByteArray("@b@%0@n\"") + s.label.toLatin1() + "\"@a@c";
    b.replace(68, message.size(), message);
    return b;
}
void CustomSquads::set(const CustomSquad &value, int classCount) {
    const auto row = CustomSquad::parse(value.json(), classCount);
    profiles_[key(row.scenarioKey, row.terrainSlot)] = row;
}
QJsonObject CustomSquads::json() const {
    QJsonArray list;
    for (const auto &p : profiles_)
        list << p.json();
    return {{"version", 1}, {"profiles", list}};
}
void CustomSquads::apply(const QJsonObject &o, int classCount) {
    if (o.size() != 2 || o["version"] != 1 || !o["profiles"].isArray() ||
        o["profiles"].toArray().size() > 2560)
        throw Error("Invalid saved custom squads");
    auto next = *this;
    next.profiles_.clear();
    for (const auto &v : o["profiles"].toArray()) {
        if (!v.isObject())
            throw Error("Invalid saved custom squad");
        const auto row = CustomSquad::parse(v.toObject(), classCount);
        if (next.profiles_.contains(key(row.scenarioKey, row.terrainSlot)))
            throw Error("Duplicate custom squad scenario and terrain slot");
        next.set(row, classCount);
    }
    profiles_ = std::move(next.profiles_);
}
int CustomSquads::changedRows() const {
    QMap<int, QJsonObject> prior;
    for (const auto &v : original_["profiles"].toArray()) {
        const auto o = v.toObject();
        prior[key(o["scenarioKey"].toInt(), o["terrainSlot"].toInt())] = o;
    }
    int count = 0;
    for (auto it = profiles_.begin(); it != profiles_.end(); ++it) {
        if (prior.take(it.key()) != it.value().json())
            ++count;
    }
    return count + int(prior.size());
}
void CustomSquads::validateFormations(const ClassData &classes) const {
    for (const auto &row : profiles_) {
        const auto s = CustomSquad::parse(row.json(), int(classes.entries().size()));
        int cost = 0;
        for (const auto &member : s.members) {
            const auto size = u8(classes.entries()[member.classId].record, 4);
            if (size > 2)
                throw Error("Custom formation uses a class with an unknown unit size");
            cost += size == 2 ? 2 : 1;
            if (size != 2)
                continue;
            for (const auto &other : s.members)
                if (member.cell != other.cell && std::abs(member.cell / 3 - other.cell / 3) <= 1 &&
                    std::abs(member.cell % 3 - other.cell % 3) <= 1)
                    throw Error("A large custom-squad member needs adjacent formation cells clear");
        }
        if (cost > 5)
            throw Error("Custom squad exceeds the five-slot formation capacity");
    }
}
QByteArray CustomSquads::bytes(quint32 offset) const {
    if (profiles_.isEmpty())
        return {};
    const auto size =
        CustomPlacement::indexBytes + profiles_.size() * CustomPlacement::profileBytes;
    if (offset < 0x2800000 || offset % 16 || quint64(offset) + size > 0x4000000)
        throw Error("Custom squad library exceeds cartridge bounds");
    QByteArray data(size, 0);
    int number = 0;
    for (auto it = profiles_.begin(); it != profiles_.end(); ++it) {
        const int at = CustomPlacement::indexBytes + number++ * CustomPlacement::profileBytes;
        put32(data, it.key() * 4, offset + at);
        data.replace(at, CustomPlacement::profileBytes, it.value().bytes());
    }
    return data;
}
CustomSquads CustomSquads::load(const Rom &rom, int classCount) {
    CustomSquads result;
    if (const auto placement = CustomPlacement::read(rom)) {
        verifyCustomCallers(rom, placement);
        for (quint32 i = 0; i < placement->profileCount; ++i) {
            const auto b = rom.bytes().mid(placement->dataOffset + CustomPlacement::indexBytes +
                                               i * CustomPlacement::profileBytes,
                                           CustomPlacement::profileBytes);
            CustomSquad row;
            row.scenarioKey = u8(b, 5);
            row.terrainSlot = u8(b, 6);
            if (u8(b, 0) > 1 || u8(b, 2) > 1 || u8(b, 1) < 2 || u8(b, 1) > 5)
                throw Error("Unsupported native custom squad flags or count");
            row.enabled = u8(b, 0);
            row.fixedPersuasion = u8(b, 2);
            row.chance = u8(b, 3);
            row.retreatHp = u8(b, 4);
            row.members.clear();
            for (int m = 0; m < u8(b, 1); ++m) {
                const int at = 8 + m * 8, raw = u8(b, at + 1);
                row.members << CustomMember{u8(b, at),
                                            raw < 128 ? raw : raw - 256,
                                            u8(b, at + 2),
                                            {u16(b, at + 4), u16(b, at + 6)}};
            }
            for (int n = 0; n < 3; ++n) {
                if (u8(b, 48 + n * 2) || u8(b, 49 + n * 2))
                    row.bonuses << qMakePair(int(u8(b, 48 + n * 2)), int(u8(b, 49 + n * 2)));
                row.rewards.outcomes[n] = u16(b, 56 + n * 4);
                row.rewards.weights[n] = u16(b, 58 + n * 4);
            }
            const auto message = b.mid(68, 40);
            const int end = message.indexOf(char(0));
            const auto framed = message.first(end < 0 ? message.size() : end);
            const QByteArray prefix("@b@%0@n\""), suffix("\"@a@c");
            if (end < 0 || !framed.startsWith(prefix) || !framed.endsWith(suffix))
                throw Error("Unsupported custom encounter card encoding");
            row.label = QString::fromLatin1(
                framed.mid(prefix.size(), framed.size() - prefix.size() - suffix.size()));
            result.set(row, classCount);
            if (result.profiles_[key(row.scenarioKey, row.terrainSlot)].bytes() != b)
                throw Error(
                    "Custom squad profile contains unsupported data; its bytes were preserved");
        }
    }
    result.original_ = result.json();
    return result;
}
void CustomSquads::importLegacy(const QJsonObject &values, int classCount) {
    auto next = *this;
    for (auto it = values.begin(); it != values.end(); ++it) {
        const auto parts = it.key().split(':');
        bool keyOk = false, slotOk = false;
        const int scenario = parts.value(0).toInt(&keyOk), slot = parts.value(1).toInt(&slotOk);
        if (parts.size() != 2 || !keyOk || !slotOk ||
            it.key() != QString::number(scenario) + ":" + QString::number(slot))
            throw Error("Invalid browser custom squad key");
        key(scenario, slot);
        if (it.value().isNull()) {
            next.remove(scenario, slot);
            continue;
        }
        if (!it.value().isObject())
            throw Error("Invalid browser custom squad");
        const auto o = it.value().toObject();
        const QStringList known{"profile_id", "runtime_key", "slice",     "terrain_slot",
                                "label",      "members",     "equipment", "persuasion",
                                "retreat",    "rewards"};
        for (auto field = o.begin(); field != o.end(); ++field)
            if (!known.contains(field.key()))
                throw Error("Unsupported browser custom squad field: " + field.key());
        if ((o.contains("runtime_key") && integer(o["runtime_key"], 0, 255) != scenario) ||
            (o.contains("terrain_slot") && integer(o["terrain_slot"], 0, 9) != slot) ||
            !o["members"].isArray())
            throw Error("Browser custom squad key disagrees with its record");
        CustomSquad row;
        row.scenarioKey = scenario;
        row.terrainSlot = slot;
        row.members.clear();
        if (o.contains("label")) {
            if (!o["label"].isString())
                throw Error("Invalid browser encounter card");
            row.label = o["label"].toString();
        }
        if (o.contains("equipment") && !o["equipment"].isObject())
            throw Error("Invalid browser squad equipment");
        const auto equipment = o["equipment"].toObject();
        for (auto field = equipment.begin(); field != equipment.end(); ++field)
            if (field.key() != "A" && field.key() != "B" && field.key() != "C")
                throw Error("Unsupported browser equipment cohort");
        for (const auto &v : o["members"].toArray()) {
            if (!v.isObject())
                throw Error("Invalid browser squad member");
            const auto m = v.toObject();
            if (m.size() != 4)
                throw Error("Unsupported browser custom member fields");
            const auto cohort = m["cohort"].toString();
            if (cohort != "A" && cohort != "B" && cohort != "C")
                throw Error("Invalid browser squad cohort");
            const auto eq =
                equipment.contains(cohort) ? equipment[cohort].toArray() : QJsonArray{0, 0};
            if (eq.size() != 2)
                throw Error("Browser cohort equipment needs two choices");
            const auto raw = integer(m["level_offset_raw"], 0, 255);
            row.members << CustomMember{integer(m["class_id"], 2, classCount - 1),
                                        raw < 128 ? raw : raw - 256,
                                        integer(m["cell"], 0, 8),
                                        {integer(eq[0], 0, 510), integer(eq[1], 0, 510)}};
        }
        for (const auto &name : {"persuasion", "retreat", "rewards"})
            if (o.contains(name) && !o[name].isObject())
                throw Error("Invalid browser squad behavior");
        const auto persuade = o["persuasion"].toObject(), retreat = o["retreat"].toObject();
        for (auto field = persuade.begin(); field != persuade.end(); ++field)
            if (field.key() != "mode" && field.key() != "chance" && field.key() != "class_bonuses")
                throw Error("Unsupported browser persuasion field");
        for (auto field = retreat.begin(); field != retreat.end(); ++field)
            if (field.key() != "hp_threshold")
                throw Error("Unsupported browser retreat field");
        if (persuade.contains("mode") && persuade["mode"] != "fixed")
            throw Error("Unsupported browser persuasion mode");
        if (persuade.contains("chance"))
            row.chance = integer(persuade["chance"], 0, 100);
        if (persuade.contains("class_bonuses") && !persuade["class_bonuses"].isArray())
            throw Error("Invalid browser leader bonuses");
        for (const auto &v : persuade["class_bonuses"].toArray()) {
            const auto b = v.toObject();
            row.bonuses << qMakePair(integer(b["class_id"], 2, classCount - 1),
                                     integer(b["bonus_percent"], 1, 100));
        }
        if (retreat.contains("hp_threshold"))
            row.retreatHp = integer(retreat["hp_threshold"], 0, 100);
        const auto rewards = o["rewards"].toObject();
        for (auto field = rewards.begin(); field != rewards.end(); ++field)
            if (field.key() != "slots")
                throw Error("Unsupported browser custom reward field");
        if (rewards.contains("slots")) {
            const auto outcomes = rewards["slots"].toArray();
            if (outcomes.size() != 3)
                throw Error("Browser rewards need three outcomes");
            for (int i = 0; i < 3; ++i) {
                const auto r = outcomes[i].toObject();
                row.rewards.outcomes[i] = integer(r["raw"], 0, 65535);
                row.rewards.weights[i] = integer(r["weight"], 0, 65535);
            }
        }
        next.set(row, classCount);
    }
    profiles_ = std::move(next.profiles_);
}
} // namespace lordly
