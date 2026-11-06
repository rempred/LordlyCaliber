#include "engine/scene_events.h"
#include "engine/new_campaign.h"
#include "engine/event_authoring.h"
#include "engine/retail_event_gate.h"
#include "engine/campaign_script.h"
#include "engine/event_script.h"
#include "engine/legacy_scene_arena.h"
#include "engine/scenario_resources.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <functional>
namespace lordly {
namespace {
const QJsonObject &catalog() {
    static const QJsonObject value = [] {
        QFile file(":/game/director/events.json");
        if (!file.open(QIODevice::ReadOnly))
            throw Error("Missing event launch catalog");
        const auto object = QJsonDocument::fromJson(file.readAll()).object();
        if (object["format"] != "lordly-event-sites" || object["version"] != 1 ||
            object["rows"].toArray().size() != SceneEvents::retailRowCount ||
            object["sites"].toArray().size() != 1998)
            throw Error("Invalid event launch catalog");
        return object;
    }();
    return value;
}
QJsonObject rowInfo(int row) {
    const auto rows = catalog()["rows"].toArray();
    if (row < 0 || row >= rows.size() || !rows[row].isObject())
        throw Error("Event row is absent from the catalog");
    return rows[row].toObject();
}
QByteArray decode(const Rom &rom, quint32 key) {
    const quint64 at = quint64(key) + SceneEvents::base;
    if (!key || key >= 0x10000000 || at + 8 > quint64(rom.bytes().size()))
        throw Error("Event row key is outside the ROM");
    const auto size = u32(rom.bytes(), at), decoded = u32(rom.bytes(), at + 4);
    if (size < 4 || size > 1024 * 1024 || !decoded || decoded > 131070 || decoded % 2 ||
        at + 4 + size > quint64(rom.bytes().size()))
        throw Error("Event row exceeds its resource or cursor bounds");
    return decodeLzss(rom.bytes().mid(at + 8, size - 4), decoded);
}
bool isMissionVictory(const QJsonObject &tree) {
    return tree["id"] == "mission-victory" && tree["group"] == 5;
}
bool unlockBeforeCompletion(const QJsonValue &mission, const QJsonValue &unlocks,
                            const QJsonArray &trees) {
    if (mission.isUndefined() || unlocks.isUndefined())
        return false;
    for (const auto value : trees) {
        const auto tree = value.toObject();
        const auto nodes = tree["nodes"].toArray();
        if (isMissionVictory(tree) && !nodes.isEmpty() &&
            nodes.last().toObject()["type"] == "completeMission")
            return true;
    }
    return false;
}
void migrateMissionCompletion(QJsonObject &flags) {
    for (auto it = flags.begin(); it != flags.end(); ++it) {
        auto edits = it.value().toObject();
        if (!edits.contains("missionName"))
            continue;
        auto trees = edits["trees"].toArray();
        for (qsizetype i = 0; i < trees.size(); ++i) {
            auto tree = trees[i].toObject();
            if (!isMissionVictory(tree))
                continue;
            auto nodes = tree["nodes"].toArray();
            if (nodes.isEmpty())
                continue;
            auto terminal = nodes.last().toObject();
            if (terminal["type"] != "battleResults" && terminal["type"] != "returnToMap")
                continue;
            terminal["type"] = "completeMission";
            nodes.replace(nodes.size() - 1, terminal);
            tree["nodes"] = nodes;
            trees.replace(i, tree);
            edits["trees"] = trees;
            it.value() = edits;
        }
    }
}
bool hasExpandedMissionRows(const QJsonObject &flags) {
    const auto rows = SceneEvents::flagRows(flags);
    for (int row : rows)
        if (row >= SceneEvents::firstCustomRow)
            return true;
    return false;
}
} // namespace
const QMap<QString, SceneEventSite> &SceneEvents::sites() {
    static const QMap<QString, SceneEventSite> result = [] {
        QMap<QString, SceneEventSite> rows;
        for (const auto value : catalog()["sites"].toArray()) {
            const auto item = value.toObject();
            SceneEventSite site{item["launchSiteId"].toString(),  item["name"].toString(),
                                item["role"].toString(),          item["mission"].toString(),
                                item["retailSceneId"].toString(), item["row"].toInt(),
                                item["offset"].toInt(),           item["selector"].toInt(),
                                item["entryCursor"].toInt(),      item["form"] == "short",
                                item["shared"].toBool()};
            if (site.id.isEmpty() || rows.contains(site.id) || site.row < 0 || site.row >= retailRowCount ||
                site.offset < (site.shortForm ? 2 : 4) || site.offset % 2 || site.selector < 0 ||
                site.selector > 65535)
                throw Error("Invalid or duplicated event launch identity");
            rows[site.id] = site;
        }
        return rows;
    }();
    return result;
}
const SceneEventSite &SceneEvents::site(const QString &id) {
    const auto found = sites().constFind(id);
    if (found == sites().cend())
        throw Error("Event launch site is not in the catalog");
    return found.value();
}
const QVector<SceneEventContext> &SceneEvents::contexts() {
    static const QVector<SceneEventContext> result = [] {
        QVector<SceneEventContext> result;
        for (const auto &v : catalog()["contexts"].toArray()) {
            const auto row = v.toObject();
            SceneEventContext context;
            context.id = row["id"].toString();
            context.site = row["site"].toString();
            context.parent = row["parent"].toString();
            context.invocation = row["invocation"].toInt(-1);
            context.parentInvocation = row["parentInvocation"].toInt(-1);
            context.ticks = qMax(1, row["ticks"].toInt(1));
            const auto translations = row["translations"].toArray();
            for (int i = 0; i < translations.size() && i < 256; ++i)
                if (translations[i].isDouble())
                    context.translations.set(i, quint16(translations[i].toInt()));
            result << context;
        }
        return result;
    }();
    return result;
}
quint32 SceneEvents::retailKey(int row) {
    if(customMissionRow(row))return 0;
    return quint32(rowInfo(row)["key"].toInteger());
}
QByteArray SceneEvents::retailRow(const Rom &rom, int row) {
    // Custom mission programs have no compressed preimage.
    // All six native handler lists initially point at one empty list.
    if(customMissionRow(row))return QByteArray::fromHex("000c000c000c000c000c000cffff");
    const auto info = rowInfo(row);
    const auto key = retailKey(row);
    if (u32(rom.bytes(), base + key) != quint32(info["storedLength"].toInteger()))
        throw Error("The preserved event preimage has changed");
    const auto bytes = decode(rom, key);
    if (bytes.size() != info["decodedLength"].toInt() ||
        sha256(bytes).toUpper() != info["decodedSha256"].toString().toLatin1())
        throw Error("The preserved event preimage differs from the audited program");
    return bytes;
}
QByteArray SceneEvents::readRow(const Rom &rom, int row) {
    if (row < 0 || row >= SceneProgramResources::eventRowCount(rom))
        throw Error("Invalid event directory row");
    return decode(rom, u32(rom.bytes(), SceneProgramResources::eventWord(row)));
}
QByteArray SceneEvents::bindRow(const QByteArray &retail, int row,
                                const QMap<QString, int> &bindings,
                                const QSet<QString> &forceStubs) {
    auto output = retail;
    if (retail.size() % 2)
        throw Error("Event program must contain complete word instructions");
    for (auto it = bindings.cbegin(); it != bindings.cend(); ++it) {
        const auto &entry = site(it.key());
        if (entry.row != row)
            continue;
        const auto at = entry.offset, selector = it.value();
        if (selector < 0 || selector > 65535)
            throw Error("Scene selector exceeds the event operand");
        if (entry.shortForm) {
            if (u8(retail, at - 2) != 0x68 || u8(retail, at - 1) != entry.selector ||
                u8(retail, at) != 0x10)
                throw Error("Short launch no longer matches its audited instruction sequence");
            if (selector <= 255 && !forceStubs.contains(entry.id)) {
                put8(output, at - 1, quint8(selector));
                continue;
            }
            const auto stub = output.size();
            if (stub + 10 > 131070)
                throw Error("Event launch stub exceeds the sixteen-bit cursor");
            const auto out = quint16((stub - (at + 2)) / 2),
                       back = quint16((at + 2 - (stub + 10)) / 2);
            output.replace(at - 2, 4, QByteArray::fromHex("68000200"));
            put8(output, at - 1, quint8(out));
            put8(output, at + 1, quint8(out >> 8));
            QByteArray code = QByteArray::fromHex("68008800100068000200");
            put8(code, 1, quint8(selector));
            put8(code, 3, quint8(selector >> 8));
            put8(code, 5, u8(retail, at + 1));
            put8(code, 7, quint8(back));
            put8(code, 9, quint8(back >> 8));
            output += code;
        } else {
            if (u8(retail, at - 4) != 0x68 || u8(retail, at - 3) != (entry.selector & 255) ||
                u8(retail, at - 2) != 0x88 || u8(retail, at - 1) != (entry.selector >> 8) ||
                u8(retail, at) != 0x10 || u8(retail, at + 1) != 0)
                throw Error("Long launch no longer matches its audited instruction sequence");
            put8(output, at - 3, quint8(selector));
            put8(output, at - 1, quint8(selector >> 8));
        }
    }
    return output;
}
QMap<QString, int> SceneEvents::discoverRow(const QByteArray &retail, const QByteArray &current,
                                            int row) {
    QMap<QString, int> bindings;
    QSet<QString> stubs;
    for (const auto &entry : sites()) {
        if (entry.row != row)
            continue;
        const auto at = entry.offset;
        int selector;
        if (!entry.shortForm)
            selector = u8(current, at - 3) | (int(u8(current, at - 1)) << 8);
        else if (u8(current, at) == 0x10)
            selector = u8(current, at - 1);
        else {
            if (u8(current, at - 2) != 0x68 || u8(current, at) != 2)
                throw Error("Unknown event launch rewrite");
            const auto relative = quint16(u8(current, at - 1) | (int(u8(current, at + 1)) << 8));
            const auto stub = int(quint16((at + 2) / 2 + relative)) * 2;
            if (stub < retail.size() || stub + 10 > current.size())
                throw Error("Event launch stub is outside its allocation");
            stubs.insert(entry.id);
            selector = u8(current, stub + 1) | (int(u8(current, stub + 3)) << 8);
        }
        if (selector != entry.selector || stubs.contains(entry.id))
            bindings[entry.id] = selector;
    }
    if (bindRow(retail, row, bindings, stubs) != current)
        throw Error("Event program contains edits outside supported launch bindings");
    for (auto it = bindings.begin(); it != bindings.end();) {
        if (it.value() == site(it.key()).selector)
            it = bindings.erase(it);
        else
            ++it;
    }
    return bindings;
}
SceneEvents SceneEvents::load(Rom rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Event bindings require US revision zero");
    const int count = SceneProgramResources::eventRowCount(rom);
    SceneEvents result(rom);
    const auto placement = SceneProgramResources::placement(rom);
    if (!placement || !placement->legacyArenaRetired)
        LegacySceneArena::read(rom);
    if (placement) {
        result.bindings_ = placement->eventBindings;
        result.flags_ = placement->eventFlags;
    }
    for (int row = 0; row < count; ++row) {
        const auto info = row < retailRowCount ? catalog()["rows"].toArray()[row].toObject() : QJsonObject{};
        const auto key = u32(rom.bytes(), SceneProgramResources::eventWord(row));
        if (key == quint32(info["key"].toInteger()))
            continue;
        if (placement && placement->eventRows.contains(row))
            continue;
        if(customMissionRow(row))throw Error("An unrecognized event program already occupies a custom mission slot");
        const auto found = discoverRow(retailRow(rom, row), readRow(rom, row), row);
        for (auto it = found.cbegin(); it != found.cend(); ++it)
            result.bindings_[it.key()] = it.value();
    }
    result.original_ = result.bindings_;
    result.originalFlags_ = result.flags_;
    migrateMissionCompletion(result.flags_);
    return result;
}
QSet<int> SceneEvents::flagRows(const QJsonObject &flags) {
    QSet<int> rows;
    for (auto it = flags.begin(); it != flags.end(); ++it) {
        bool ok; const int row = it.key().toInt(&ok);
        if (!ok || QString::number(row) != it.key() || row < 0 || row >= rowCapacity ||
            !it.value().isObject() || it.value().toObject().isEmpty())
            throw Error("Invalid event flag row");
        rows.insert(row);
    }
    return rows;
}
QByteArray SceneEvents::flagRow(const QByteArray &retail, int row, const QJsonObject &flags,bool structural) {
    auto edits=flags[QString::number(row)].toObject();
    const auto replacement=edits.take("replace");
    if(!replacement.isUndefined()&&(!replacement.isBool()||!replacement.toBool()))throw Error("Invalid event replacement");
    const auto campaign=edits.take("campaign");
    if(!campaign.isUndefined()) {
        if(row!=1||!campaign.isObject())throw Error("Campaign settings belong to New Game");
        NewCampaign::validate(campaign.toObject());
    }
    QByteArray base=retail;
    if(replacement.toBool()) {
        base=row==0?QByteArray(110,0):row==70?QByteArray(108,0):QByteArray::fromHex("000c000c000c000c000c000cffff");
        if(row==70)put16(base,106,65535);
        if(row==0) {
            // Selector0 is the real New Game entry, not a physical location.
            // Keep only its flag99 guard; all retail location/chapter paths go.
            put16(base,0,110);put16(base,108,65535);
            base+=QByteArray::fromHex("69636061ac0101036800880012012f00");
        }
    }
    const auto mission=edits.take("missionName");
    if(!mission.isUndefined()&&(!customMissionRow(row)||!mission.isString()||
       mission.toString().trimmed().isEmpty()||mission.toString().size()>100))throw Error("Invalid custom mission name");
    if(row>=firstCustomRow&&mission.isUndefined())throw Error("An expanded event row needs a custom mission name");
    const auto customFlags=edits.take("customFlags");
    if(!customFlags.isUndefined()&&(row!=0||!customFlags.isArray()))throw Error("Invalid custom story flag definitions");
    const auto trees=edits.take("trees"),triggers=edits.take("triggers");
    const auto gates=edits.take("gates");
    if(!gates.isUndefined()&&(!gates.isObject()||(!gates.toObject().isEmpty()&&!EventAuthoring::supported(row))))throw Error("Invalid event condition edits");
    if((!trees.isUndefined()&&!trees.isArray())||(!triggers.isUndefined()&&!triggers.isObject()))throw Error("Invalid event authoring data");
    const auto operands=edits.take("operands");
    if(!operands.isUndefined()&&!operands.isObject())throw Error("Invalid event operands");
    if(row==70 && replacement.toBool()) {
        if(!edits.isEmpty()||!trees.isUndefined()||!triggers.isUndefined()||!operands.isUndefined()||!gates.isUndefined())throw Error("Investigation routing needs location entry rules");
        return base;
    }
    if(row==0) {if(!trees.isUndefined()||!triggers.isUndefined())throw Error("Arrival events use location entry rules");return compileWorldEntries(operands.toObject().isEmpty()?base:EventScript::applyOperands(base,row,operands.toObject()),edits);}
    const auto unlocks=edits.take("unlockNodes");
    if(!unlocks.isUndefined() && !unlocks.isObject())throw Error("Invalid world-node unlock data");
    const auto authoredTrees = trees.toArray();
    const bool completesMission = unlockBeforeCompletion(mission, unlocks, authoredTrees);
    auto source = EventScript::applyOperands(EventScript::apply(base, edits), row, operands.toObject());
    if (!completesMission)
        source = appendWorldUnlocks(source, unlocks.toObject());
    auto compiled = EventAuthoring::compile(source, row, authoredTrees, triggers.toObject());
    if (completesMission)
        compiled = appendWorldUnlocks(compiled, unlocks.toObject());
    return structural?RetailEventGate::compile(compiled,base,row,gates.toObject()):compiled;
}
QByteArray SceneEvents::program(int row,bool structural) const {
    return bindRow(flagRow(retailRow(rom_, row), row, flags_,structural), row, bindings_);
}
QByteArray SceneEvents::authoredBase(int row) const {
    const auto rowKey = QString::number(row);
    auto edits = flags_[rowKey].toObject();
    const auto trees = edits.take("trees").toArray();
    edits.remove("triggers");
    edits.remove("gates");
    if (unlockBeforeCompletion(edits.value("missionName"), edits.value("unlockNodes"), trees))
        edits.remove("unlockNodes");
    return flagRow(retailRow(rom_, row), row, {{rowKey, edits}}, false);
}
QVector<int> SceneEvents::scenarioRows(int key) const {
    QVector<int> result;
    if (key < 0 || key > 255) return result;
    auto match = QByteArray::fromHex("680070e970e7"); match[1] = char(key);
    for (int row : {2, 3})
        if (!missionName(row).isEmpty() && program(row).contains(match))
            result << row;
    for (int row = 4; row < retailRowCount; ++row) {
        if (!catalog()["rows"].toArray()[row].isObject()) continue;
        const auto bytes = flags_[QString::number(row)].toObject()["replace"].toBool()?program(row):retailRow(rom_, row);
        for (int at = 0; at + match.size() <= bytes.size(); at += 2)
            if (bytes.mid(at, match.size()) == match) { result << row; break; }
    }
    for (int row = firstCustomRow; row < rowCapacity; ++row)
        if (!missionName(row).isEmpty() && program(row).contains(match))
            result << row;
    return result;
}
void SceneEvents::editFlag(int row, int offset, int flag, bool value) {
    const auto retail = retailRow(rom_, row);
    const auto controls = EventScript::flagSteps(retail);
    if (!controls.contains(offset)) throw Error("This event command is not an editable flag control");
    auto edits = flags_.value(QString::number(row)).toObject();
    auto operands=edits["operands"].toObject();
    operands.remove(QString::number(offset));
    if(controls[offset].check)operands.remove(QString("comparison:%1").arg(offset+4));
    if(!controls[offset].check)operands.remove(QString::number(offset+2));
    if(operands.isEmpty())edits.remove("operands");else edits["operands"]=operands;
    auto instruction = edits[QString::number(offset)].toObject();
    if (controls[offset].flag == flag && controls[offset].value == value) {
        instruction.remove("flag"); instruction.remove("value");
    } else { instruction["flag"] = flag; instruction["value"] = value; }
    if (instruction.isEmpty()) edits.remove(QString::number(offset));
    else edits[QString::number(offset)] = instruction;
    auto check=flags_;check[QString::number(row)]=edits;flagRow(retail,row,check);
    if (edits.isEmpty()) flags_.remove(QString::number(row));
    else flags_[QString::number(row)] = edits;
}
void SceneEvents::editBranch(int row, int offset, int target) {
    const auto retail = retailRow(rom_, row);
    const auto controls = EventScript::branchSteps(retail);
    if (!controls.contains(offset)) throw Error("This instruction is not a branch");
    auto edits = flags_[QString::number(row)].toObject();
    auto operands=edits["operands"].toObject();operands.remove(QString::number(offset+controls[offset].length-2));
    if(operands.isEmpty())edits.remove("operands");else edits["operands"]=operands;
    auto instruction = edits[QString::number(offset)].toObject();
    if (target == controls[offset].target) instruction.remove("target");
    else instruction["target"] = target;
    if (instruction.isEmpty()) edits.remove(QString::number(offset));
    else edits[QString::number(offset)] = instruction;
    auto check=flags_;check[QString::number(row)]=edits;flagRow(retail,row,check);
    if (edits.isEmpty()) flags_.remove(QString::number(row));
    else flags_[QString::number(row)] = edits;
}
void SceneEvents::editParameter(int row,int offset,const QString &key,int value,const ScenePrograms &scenes) {
    const auto retail=retailRow(rom_,row);
    // Scene selectors use the existing relocatable launch binding path, including
    // widening a one-byte selector into a stub. Never give two writers that operand.
    for(const auto &launch:sites())if(launch.row==row) {
        const int low=launch.offset-(launch.shortForm?2:4),high=launch.offset-2;
        if((offset==low&&key=="value")||(!launch.shortForm&&offset==high&&key=="value")) {
            const int selector=offset==high&&!launch.shortForm ? (target(launch.id)&255)|(value<<8) : value;
            bind(launch.id,selector,scenes);return;
        }
    }
    auto next=*this;
    const auto current=flagRow(retail,row,flags_,false);
    const auto patch=EventScript::parameterEdit(current,row,offset,key,value);
    auto edits=flags_[QString::number(row)].toObject();
    auto operands=edits["operands"].toObject();
    auto without=edits;without.remove("operands");auto baseFlags=flags_;baseFlags[QString::number(row)]=without;
    const auto baseBytes=flagRow(retail,row,baseFlags,false);
    for(auto p=patch.begin();p!=patch.end();++p) {
        const bool comparison=p.key().startsWith("comparison:");
        const int at=comparison?p.key().mid(11).toInt():p.key().toInt();
        const int baseline=comparison?u8(baseBytes,at)>>4:u8(baseBytes,at+1);
        if(p.value().toInt()==baseline)operands.remove(p.key());else operands[p.key()]=p.value();
    }
    if(operands.isEmpty())edits.remove("operands");else edits["operands"]=operands;
    if(edits.isEmpty())next.flags_.remove(QString::number(row));else next.flags_[QString::number(row)]=edits;
    next.validate(scenes);*this=std::move(next);
}
void SceneEvents::restoreFlags(int row) {
    const auto original = originalFlags_.value(QString::number(row));
    if (original.isUndefined()) flags_.remove(QString::number(row));
    else flags_[QString::number(row)] = original;
}
void SceneEvents::setWorldRules(int node, const QJsonArray &rules) {
    const auto choices=flagChoices();
    for (const auto rule : rules) {
        const auto entry = rule.toObject();
        const int row = entry["event"].toInt(-1);
        if (row >= firstCustomRow && missionName(row).isEmpty())
            throw Error("The arrival rule refers to a missing custom mission");
        for (const auto condition : entry["conditions"].toArray())
            if (!choices.contains(condition.toObject()["flag"].toInt(-1)))
                throw Error("The arrival rule refers to a missing flag");
    }
    auto entries=flags_["0"].toObject();
    if(rules.isEmpty())entries.remove(QString::number(node));else entries[QString::number(node)]=rules;
    auto test=flags_;test["0"]=entries;flagRow(retailRow(rom_,0),0,test);
    if(entries.isEmpty())flags_.remove("0");else flags_["0"]=entries;
}
QString SceneEvents::missionName(int row) const {
    return flags_[QString::number(row)].toObject()["missionName"].toString();
}
int SceneEvents::createMission(int location,int key,const QString &name,int openingScene,int victoryScene,
                               const ScenePrograms &scenes,int prerequisiteFlag) {
    if(location<1||location>40||key<1||key>=ScenarioResources::keyCount||name.trimmed().isEmpty()||name.size()>100)
        throw Error("Choose a map location, battlefield and scenario name");
    int row = firstCustomRow;
    while (row < rowCapacity && flags_.contains(QString::number(row)))
        ++row;
    if (row == rowCapacity)
        throw Error("All 64 new mission event programs are in use");
    auto next=*this;
    const int flag=next.createFlag(name.trimmed()+" completed");
    next.flags_[QString::number(row)]=QJsonObject{{"missionName",name.trimmed()}};
    next.setTrees(row,EventAuthoring::missionTemplates(key,location,flag,openingScene,victoryScene),scenes);
    QJsonArray conditions;
    if (prerequisiteFlag != -1) {
        if (!flagChoices().contains(prerequisiteFlag))
            throw Error("Choose an existing prerequisite flag");
        conditions.append(QJsonObject{{"flag", prerequisiteFlag}, {"value", true}});
    }
    conditions.append(QJsonObject{{"flag", flag}, {"value", false}});
    auto rules = next.flags_["0"].toObject()[QString::number(location)].toArray();
    rules.append(QJsonObject{{"conditions",conditions},{"event",row},{"clearFrom",0},{"clearTo",0}});
    next.setWorldRules(location, rules);
    next.validate(scenes);*this=std::move(next);return row;
}
void SceneEvents::setTrees(int row,const QJsonArray &trees,const ScenePrograms &scenes){
    auto next=*this;auto edits=next.flags_[QString::number(row)].toObject();
    if(row==1&&!edits["campaign"].toObject().isEmpty()) {
        for(const auto &value:trees) {
            const auto tree=value.toObject();if(tree["id"]!="campaign-opening")continue;
            const auto nodes=tree["nodes"].toArray();
            if(nodes.size()>1&&nodes[1].toObject()["type"]=="startingArmy") {
                auto config=edits["campaign"].toObject();config["army"]=nodes[1].toObject()["army"];edits["campaign"]=config;
            }
        }
    }
    if(trees.isEmpty())edits.remove("trees");else edits["trees"]=trees;
    if(edits.isEmpty())next.flags_.remove(QString::number(row));else next.flags_[QString::number(row)]=edits;
    next.validate(scenes);*this=std::move(next);
}
void SceneEvents::setRetailGate(int row,int comparison,const QJsonObject &gate,const ScenePrograms &scenes){
    auto next=*this;auto edits=next.flags_[QString::number(row)].toObject();auto gates=edits["gates"].toObject();
    if(gate.isEmpty())gates.remove(QString::number(comparison));else gates[QString::number(comparison)]=gate;
    if(gates.isEmpty())edits.remove("gates");else edits["gates"]=gates;
    if(edits.isEmpty())next.flags_.remove(QString::number(row));else next.flags_[QString::number(row)]=edits;
    next.validate(scenes);*this=std::move(next);
}
void SceneEvents::setTrigger(int row,int entry,const QJsonObject &trigger,const ScenePrograms &scenes){
    auto next=*this;auto edits=next.flags_[QString::number(row)].toObject(),triggers=edits["triggers"].toObject();
    if(trigger.isEmpty())triggers.remove(QString::number(entry));else triggers[QString::number(entry)]=trigger;
    if(triggers.isEmpty())edits.remove("triggers");else edits["triggers"]=triggers;
    if(edits.isEmpty())next.flags_.remove(QString::number(row));else next.flags_[QString::number(row)]=edits;
    next.validate(scenes);*this=std::move(next);
}
QJsonArray SceneEvents::customFlags()const{return flags_["0"].toObject()["customFlags"].toArray();}
bool SceneEvents::hasCustomFlags()const{return !customFlags().isEmpty();}
QString SceneEvents::flagName(int id,int row,int offset)const{
    if(id<firstCustomFlag)return EventScript::flagName(id,row,offset);
    for(const auto &v:customFlags()){const auto flag=v.toObject();if(flag["id"].toInt()==id&&!flag["deleted"].toBool())return QString("Flag %1 (%2)").arg(id).arg(flag["name"].toString());}
    return QString("Unknown custom flag ID %1").arg(id);
}
QMap<int,QString> SceneEvents::flagChoices(int row)const{
    QMap<int,QString> out;for(int i=0;i<256;++i)out[i]=flagName(i,row);
    for(const auto &v:customFlags()){const auto flag=v.toObject();if(!flag["deleted"].toBool())out[flag["id"].toInt()]=flagName(flag["id"].toInt(),row);}return out;
}
namespace {
QString customFlagName(const QString &value){const auto name=value.trimmed();if(name.isEmpty()||name.size()>120||name.contains('\n')||name.contains('\r'))throw Error("Enter a flag name of 1–120 characters");return name;}
void checkFlagName(const QJsonArray &flags,const QString &name,int except=-1){for(const auto &v:flags){const auto f=v.toObject();if(f["id"].toInt()!=except&&!f["deleted"].toBool()&&f["name"].toString().compare(name,Qt::CaseInsensitive)==0)throw Error("A custom flag already uses that name");}}
}
int SceneEvents::createFlag(const QString &value){
    const auto name=customFlagName(value);auto flags=customFlags();checkFlagName(flags,name);int id=firstCustomFlag;
    for(const auto &v:flags)id=qMax(id,v.toObject()["id"].toInt()+1);
    if(id>=firstCustomFlag+customFlagCapacity)throw Error("All 512 custom flag IDs have been allocated");
    flags<<QJsonObject{{"id",id},{"name",name}};auto row=flags_["0"].toObject();row["customFlags"]=flags;flags_["0"]=row;return id;
}
void SceneEvents::renameFlag(int id,const QString &value){
    const auto name=customFlagName(value);auto flags=customFlags();checkFlagName(flags,name,id);bool found=false;
    for(int i=0;i<flags.size();++i){auto f=flags[i].toObject();if(f["id"].toInt()==id&&!f.value("deleted").toBool()){f["name"]=name;flags[i]=f;found=true;break;}}
    if(!found)throw Error("Select a custom flag");auto row=flags_["0"].toObject();row["customFlags"]=flags;flags_["0"]=row;
}
void SceneEvents::removeFlag(int id){
    const auto arrivals=flags_["0"].toObject();
    for(auto rules:arrivals)for(auto rule:rules.toArray())for(auto condition:rule.toObject()["conditions"].toArray())
        if(condition.toObject()["flag"].toInt(-1)==id)throw Error("This flag is used by a map location entry rule");
    std::function<bool(const QJsonValue &)> used=[&](const QJsonValue &v){if(v.isArray()){for(const auto &x:v.toArray())if(used(x))return true;}else if(v.isObject()){const auto o=v.toObject();if(o["type"]=="flag"&&o["flag"].toInt(-1)==id)return true;for(auto i=o.begin();i!=o.end();++i)if(used(i.value()))return true;}return false;};
    for(auto row=flags_.begin();row!=flags_.end();++row)if(used(row.value().toObject()["trees"]))throw Error("This flag is used by an event tree");
    for(auto row:flags_)if(used(row.toObject()["gates"]))throw Error("This flag is used by a scenario condition");
    auto definitions=customFlags();bool found=false;for(int i=0;i<definitions.size();++i){auto flag=definitions[i].toObject();if(flag["id"].toInt()==id&&!flag["deleted"].toBool()){flag["deleted"]=true;definitions[i]=flag;found=true;break;}}
    if(!found)throw Error("Select a custom flag");auto row=flags_["0"].toObject();row["customFlags"]=definitions;flags_["0"]=row;
}
void SceneEvents::unlockNodeAfter(int row,int node,bool enabled) {
    if(row<2 || row>=rowCapacity)throw Error("Choose a mission event row for the unlock");
    auto edits=flags_[QString::number(row)].toObject();auto unlocks=edits["unlockNodes"].toObject();
    if(enabled)unlocks[QString::number(node)]=true;else unlocks.remove(QString::number(node));
    if(unlocks.isEmpty())edits.remove("unlockNodes");else edits["unlockNodes"]=unlocks;
    auto check=flags_;check[QString::number(row)]=edits;flagRow(retailRow(rom_,row),row,check);
    if(edits.isEmpty())flags_.remove(QString::number(row));else flags_[QString::number(row)]=edits;
}
int SceneEvents::target(const QString &id) const {
    return bindings_.value(id, site(id).selector);
}
void SceneEvents::bind(const QString &id, int selector, const ScenePrograms &scenes) {
    const auto &entry = site(id);
    auto next = *this;
    if (selector == entry.selector)
        next.bindings_.remove(id);
    else
        next.bindings_[id] = selector;
    next.validate(scenes);
    *this = std::move(next);
}
void SceneEvents::restore(const QString &id) {
    site(id);
    bindings_.remove(id);
}
void SceneEvents::validate(const ScenePrograms &scenes) const {
    const auto campaign=NewCampaign::settings(*this);
    NewCampaign::validate(campaign);
    if(!campaign.isEmpty()) {
        bool openingFound=false;
        for(const auto &value:flags_["1"].toObject()["trees"].toArray()) {
            const auto tree=value.toObject();if(tree["id"]!="campaign-opening")continue;
            openingFound=true;const auto nodes=tree["nodes"].toArray();
            if(tree["group"].toInt(-1)!=0||nodes.size()<3||
               nodes.first().toObject()["type"]!="initializeCampaign"||
               nodes[1].toObject()["type"]!="startingArmy"||
               nodes.last().toObject()["type"]!="beginCampaign")
                throw Error("Keep Initialize campaign settings and the default starting army first, and Begin campaign on world map last. Add opening choices between them.");
        }
        if(!openingFound)throw Error("New Campaign needs its opening event");
    }
    std::function<void(const QJsonValue &,int)> validateCampaignAction=[&](const QJsonValue &value,int row) {
        if(value.isArray())for(const auto &v:value.toArray())validateCampaignAction(v,row);
        if(value.isObject()) {
            const auto n=value.toObject();const auto type=n["type"].toString();
            if(type=="startingArmy"||type=="beginCampaign"||type=="initializeCampaign") {
                if(row!=1||campaign.isEmpty())throw Error("Starting armies can only be assigned in a New Campaign opening");
                if(type=="startingArmy")NewCampaign::armyBytes(campaign,n["army"].toInt(-1));
            }
            for(auto v:n)if(v.isArray()||v.isObject())validateCampaignAction(v,row);
        }
    };
    for(auto row=flags_.begin();row!=flags_.end();++row)validateCampaignAction(row.value().toObject()["trees"],row.key().toInt());
    QSet<int> customIds;QSet<QString> customNames;
    for(const auto &v:customFlags()){const auto f=v.toObject();const int id=f["id"].toInt(-1);const auto name=customFlagName(f["name"].toString());
        if(!v.isObject()||f["id"].toDouble()!=id||id<firstCustomFlag||id>=firstCustomFlag+customFlagCapacity||customIds.contains(id)||
            (f.contains("deleted")&&!f["deleted"].isBool()))throw Error("Invalid custom flag ID");
        customIds.insert(id);if(!f["deleted"].toBool()){if(customNames.contains(name.toCaseFolded()))throw Error("Duplicate custom flag name");customNames.insert(name.toCaseFolded());}}
    for (int row : flagRows(flags_)) flagRow(retailRow(rom_, row), row, flags_);
    const auto choices=flagChoices();const auto arrivals=flags_["0"].toObject();
    for (const auto rules : arrivals) {
        for (const auto rule : rules.toArray()) {
            const auto entry = rule.toObject();
            const int row = entry["event"].toInt(-1);
            if (row >= firstCustomRow && missionName(row).isEmpty())
                throw Error("A location entry rule refers to a missing custom mission");
            for (const auto condition : entry["conditions"].toArray())
                if (!choices.contains(condition.toObject()["flag"].toInt(-1)))
                    throw Error("A location entry rule refers to a missing flag");
        }
    }
    QSet<int> selectors;
    for (const auto &id : scenes.scenes())
        for (const auto selector : scenes.resource(id).selectors)
            selectors.insert(selector);
    const auto flagOptions=flagChoices();const auto availableFlags=QSet<int>(flagOptions.keyBegin(),flagOptions.keyEnd());
    for(auto row=flags_.begin();row!=flags_.end();++row)EventAuthoring::validateReferences(row.value().toObject()["trees"].toArray(),selectors,availableFlags);
    for(auto row:flags_)for(auto gate:row.toObject()["gates"].toObject())
        if(gate.toObject()["mode"]=="if")EventAuthoring::validateReferences({gate.toObject()["condition"]},selectors,availableFlags);
    for (auto it = bindings_.cbegin(); it != bindings_.cend(); ++it) {
        site(it.key());
        if (!selectors.contains(it.value()))
            throw Error(QString("Launch %1 targets an absent scene selector %2")
                            .arg(it.key())
                            .arg(it.value()));
    }
}
bool SceneEvents::hasCampaignEdits(const QJsonObject &flags) {
    if(flags.contains("0")){auto arrival=flags["0"].toObject();arrival.remove("customFlags");if(!arrival.isEmpty())return true;}
    for(auto row=flags.begin();row!=flags.end();++row) {
        const auto edits=row.value().toObject();
        if(edits.contains("unlockNodes"))return true;
        for(auto it=edits.begin();it!=edits.end();++it)if(it.value().toObject().contains("target"))return true;
    }
    return false;
}
QJsonObject SceneEvents::json() const {
    QJsonObject bindings;
    for (auto it = bindings_.cbegin(); it != bindings_.cend(); ++it)
        bindings[it.key()] = it.value();
    if (flags_.isEmpty()) return {{"version", 1}, {"bindings", bindings}};
    bool parameters=false;for(auto r=flags_.begin();r!=flags_.end();++r)parameters|=r.value().toObject().contains("operands");
    bool authored=false;for(auto r=flags_.begin();r!=flags_.end();++r)authored|=r.value().toObject().contains("trees")||r.value().toObject().contains("triggers");
    bool structural=false;for(auto row:flags_)structural|=row.toObject().contains("gates")||row.toObject().contains("missionName");
    return {{"version", !NewCampaign::settings(*this).isEmpty() ? 9 : hasExpandedMissionRows(flags_) ? 8 : structural ? 7 : hasCustomFlags() ? 6 : authored ? 5 : parameters ? 4 : hasCampaignEdits(flags_) ? 3 : 2}, {"bindings", bindings}, {"flags", flags_}};
}
void SceneEvents::apply(const QJsonObject &value, const ScenePrograms &scenes) {
    const int version = value["version"].toInt();
    if (version < 1 || version > 9 || !value["bindings"].isObject() ||
        (value["version"].toInt() >= 2 && !value["flags"].isObject()) ||
        (value["version"] == 1 && value.contains("flags")))
        throw Error("Invalid saved event bindings");
    auto next = *this;
    next.flags_ = value["flags"].toObject();
    if (version < 8 && hasExpandedMissionRows(next.flags_))
        throw Error("Expanded event rows require event project version 8");
    migrateMissionCompletion(next.flags_);
    next.bindings_.clear();
    const auto values = value["bindings"].toObject();
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        const auto selector = it.value().toInt(-1);
        if (selector < 0 || selector > 65535 || it.value().toDouble() != selector)
            throw Error("Invalid saved event target selector");
        if (selector != site(it.key()).selector)
            next.bindings_[it.key()] = selector;
    }
    next.validate(scenes);
    *this = std::move(next);
}
SceneEventBuild SceneEvents::plan() const {
    SceneEventBuild result;
    result.bindings = bindings_;
    result.flags = flags_;
    for (int row : flagRows(flags_)) result.rows[row] = program(row);
    for (auto it = bindings_.cbegin(); it != bindings_.cend(); ++it) {
        const auto row = site(it.key()).row;
        if (!result.rows.contains(row))
            result.rows[row] = program(row);
    }
    return result;
}
int SceneEvents::changedSites() const {
    QSet<QString> keys;
    for (auto it = bindings_.cbegin(); it != bindings_.cend(); ++it)
        keys.insert(it.key());
    for (auto it = original_.cbegin(); it != original_.cend(); ++it)
        keys.insert(it.key());
    int count = 0;
    for (const auto &id : keys)
        count += bindings_.value(id, site(id).selector) != original_.value(id, site(id).selector);
    auto rows = flagRows(flags_); rows.unite(flagRows(originalFlags_));
    for (int row : rows) {
        const auto before = originalFlags_[QString::number(row)].toObject(), after = flags_[QString::number(row)].toObject();
        auto offsets = before.keys();
        for (const auto &key : after.keys()) if (!offsets.contains(key)) offsets << key;
        for (const auto &offset : offsets) count += before[offset] != after[offset];
    }
    return count;
}
} // namespace lordly
