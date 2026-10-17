#include "engine/project.h"
#include "engine/weapon_status.h"
#include "engine/squad_members.h"
#include "engine/event_script.h"
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <algorithm>

namespace lordly {
namespace {
QString quoted(QString text) {
    text.replace('\\', "\\\\").replace('\r', "\\r").replace('\n', "\\n").replace('"', "\\\"");
    return '"' + text + '"';
}
QString transition(const QString &label, const QString &before, const QString &after) {
    return label + ": " + before + " → " + after;
}
QString number(int value) {
    return QString::number(value);
}
QString friendly(QString key) {
    key.replace(QRegularExpression("([a-z])([A-Z])"), "\\1 \\2");
    if (!key.isEmpty())
        key[0] = key[0].toUpper();
    return key;
}
// Used only for structured numeric settings, never binary artwork/ROM payloads.
QString scalar(const QJsonValue &value) {
    if (value.isUndefined() || value.isNull())
        return "None";
    if (value.isBool())
        return value.toBool() ? "On" : "Off";
    if (value.isString())
        return quoted(value.toString());
    return QString::number(value.toDouble(), 'g', 12);
}
void settings(QStringList &out, const QJsonValue &before, const QJsonValue &after,
              const QString &label = {}) {
    if (before == after)
        return;
    if (before.isObject() || after.isObject()) {
        const auto a = before.toObject(), b = after.toObject();
        auto keys = a.keys();
        for (const auto &key : b.keys())
            if (!keys.contains(key))
                keys << key;
        std::sort(keys.begin(), keys.end());
        for (const auto &key : keys) {
            if (key == "version" || key == "schemaVersion")
                continue;
            settings(out, a[key], b[key],
                     label.isEmpty() ? friendly(key) : label + " / " + friendly(key));
        }
    } else if (before.isArray() || after.isArray()) {
        const auto a = before.toArray(), b = after.toArray();
        for (int i = 0; i < std::max(a.size(), b.size()); ++i)
            settings(out, a.at(i), b.at(i), label + QString(" [%1]").arg(i));
    } else
        out << transition(label, scalar(before), scalar(after));
}
template <class T>
QStringList rowChanges(const QVector<T> &a, const QVector<T> &b, const QString &label) {
    QStringList lines;
    for (int i = 0; i < std::max(a.size(), b.size()); ++i) {
        if (i >= a.size())
            lines << QString("Added %1 %2").arg(label).arg(i + 1);
        else if (i >= b.size())
            lines << QString("Removed %1 %2").arg(label).arg(i + 1);
        else if (a[i] != b[i])
            lines << QString("Updated %1 %2").arg(label).arg(i + 1);
    }
    return lines;
}
int fieldValue(const QByteArray &bytes, const Field &field) {
    int value = field.width == 2 ? u16(bytes, field.offset) : u8(bytes, field.offset);
    if (field.bits < 8)
        value = (value >> field.shift) & field.maximum();
    return field.signedValue && value >= 128 ? value - 256 : value;
}
} // namespace

QString ChangeReport::text() const {
    QStringList lines{"Lordly Caliber — Project changes", "Comparison ROM: " + source,
                      "Changes are relative to this comparison ROM, including saved project edits.",
                      "Unapplied form input and the separate Save workspace are not included. Unknown game code and data are preserved, not interpreted as editable fields.", ""};
    if (entries.isEmpty())
        lines << "No project changes.";
    QString area;
    for (const auto &entry : entries) {
        if (area != entry.area) {
            area = entry.area;
            lines << "" << area << QString(area.size(), '=');
        }
        lines << entry.subject;
        for (const auto &detail : entry.details)
            lines << "  - " + detail;
    }
    return lines.join('\n') + '\n';
}

ChangeReport Project::compareWithRom(Rom rom) const {
    rom.requireSupported();
    if (rom.revision() != rom_.revision()) throw Error("Choose a comparison ROM of the same game revision.");
    auto current = *this, reference = Project::fromRom(std::move(rom), catalog_);
    auto load = [](Project &p) {
        p.loadTexts(); p.loadPromotion(); p.loadShops(); p.loadConsumableEffects(); p.loadTools();
        p.loadEncounters(); p.loadScenarios(); p.loadWorld(); p.loadTraining(); p.loadMapEdits();
        p.loadScenePrograms(); p.loadSceneEvents(); p.loadDialogues(); p.loadCategoryIcons();
        p.loadIconArtwork(); p.loadAvatarArtwork(); p.loadArmyArtwork(); p.loadCombatArtwork();
        p.loadAnimationRoutes(); p.loadAnimationOverrides(); p.loadAnimationSequences();
    };
    load(current); load(reference);
    return current.changeReport(&reference);
}

ChangeReport Project::changeReport(const Project *reference) const {
    const auto &baseRom = reference ? reference->rom_ : rom_;
    ChangeReport report;
    report.source = QFileInfo(baseRom.sourcePath()).fileName();
    if (report.source.isEmpty())
        report.source = "Loaded ROM";
    auto add = [&](QString area, QString title, QStringList details, EditLocation location = {}) {
        if (location.editor.isEmpty() && area != "Imported project data") location.editor = area;
        if (!details.isEmpty())
            report.entries << ProjectChange{area, title, details, location};
    };
    auto className = [&](int id) {
        return id >= 0 && id < records(Table::Classes).size()
                   ? QString("%1 (class %2)").arg(record(Table::Classes, id).name).arg(id)
                   : QString("Class %1").arg(id);
    };
    auto itemName = [&](int id, bool consumable = false) {
        const auto table = consumable ? Table::Consumables : Table::Items;
        const int row = id;
        if (!id)
            return QString("None");
        return row >= 0 && row < records(table).size()
                   ? QString("%1 (%2)").arg(record(table, row).name).arg(id)
                   : QString("Item %1").arg(id);
    };
    auto choice = [&](const Field &field, int value, bool original) {
        if (field.choices == "CLASS_NAMES") {
            if (original && reference && value >= 0 && value < reference->records(Table::Classes).size())
                return reference->record(Table::Classes,value).name+QString(" (%1)").arg(value);
            if (original && originalClasses_ && value >= 0 &&
                value < originalClasses_->entries().size())
                return decodeText(originalClasses_->entries()[value].name) +
                       QString(" (%1)").arg(value);
            return className(value);
        }
        if (field.choices == "ITEM_NAMES")
            return itemName(value);
        const auto label = catalog_[field.choices].toObject()[number(value)].toString();
        return label.isEmpty() ? number(value) : label + QString(" (%1)").arg(value);
    };
    for (const auto table : {Table::Classes, Table::Items, Table::Consumables}) {
        const auto area = table == Table::Classes ? "Classes"
                          : table == Table::Items ? "Items and equipment"
                                                  : "Consumables";
        for (const auto &row : records(table)) {
            const bool added = reference ? row.id >= reference->records(table).size() : (table == Table::Classes && originalClasses_ &&
                                row.id >= originalClasses_->entries().size()) ||
                               (table == Table::Items && originalItems_ &&
                                row.id >= originalItems_->entries().size());
            QStringList lines;
            if (added)
                lines << (table == Table::Classes ? "Added class; initial field values follow."
                                                  : "Added item; initial field values follow.");
            if (reference && !added && row.name != reference->record(table, row.id).name)
                lines << transition("Name", quoted(reference->record(table, row.id).name), quoted(row.name));
            if (!reference && table == Table::Classes && classNames_.contains(row.id) && !added)
                lines << transition("Name", quoted(decodeText(classAsset(row.id).name)),
                                    quoted(row.name));
            if (!reference && table == Table::Items && itemNames_.contains(row.id) && !added)
                lines << transition("Name",
                                    quoted(decodeText(originalItems_->entries()[row.id].name)),
                                    quoted(row.name));
            for (const auto &field : fields(table)) {
                if (field.offset < 4) continue; // Relocated name pointers are not user edits.
                const auto before = fieldValue(reference && !added ? reference->record(table,row.id).bytes : row.original, field),
                           after = fieldValue(row.bytes, field);
                if (added || before != after) {
                    if (table == Table::Items && field.key == "b23Raw" &&
                        (added || (before & WeaponStatus::mask) != (after & WeaponStatus::mask))) {
                        lines << (added ? "Status effect on hit: " + WeaponStatus::name(after)
                                        : transition("Status effect on hit", WeaponStatus::name(before),
                                                     WeaponStatus::name(after)));
                        if ((before & ~WeaponStatus::mask) == (after & ~WeaponStatus::mask)) continue;
                    }
                    const auto label = field.group + " / " + field.label;
                    lines << (added ? label + ": " + choice(field, after, false)
                                    : transition(label, choice(field, before, true),
                                                 choice(field, after, false)));
                }
            }
            add(area, QString("%1 (ID %2)").arg(row.name).arg(row.id), lines, {tableKey(table), row.id});
        }
    }
    for (const auto &bank : texts_) {
        for (int id = 0; id < bank.entries.size(); ++id) {
            const auto &entry = bank.entries[id];
            const auto &baseBank = reference ? reference->texts_[bank.spec.id] : bank;
            const auto original = reference && id < baseBank.entries.size() ? baseBank.entries[id].bytes : entry.original;
            const bool added = reference ? id >= baseBank.entries.size() : bank.spec.id == "classes" && originalClasses_ &&
                               id >= (originalClasses_->entries().size() < 165
                                          ? originalClasses_->entries().size()
                                          : originalClasses_->entries().size() + 60);
            if (entry.bytes == original && !added)
                continue;
            QString subject = bank.spec.label + QString(" %1").arg(id);
            int recordId = id;
            for (const auto table : {Table::Classes, Table::Items, Table::Consumables})
                if (tableKey(table) == bank.spec.id)
                    for (const auto &row : records(table))
                        if ((table == Table::Classes ? classDescriptionIndex(row.id) : row.id) ==
                            id)
                            { subject = row.name + " description"; recordId = row.id; }
            add("Descriptions", subject,
                {added ? "Added: " + quoted(decodeText(entry.bytes))
                       : transition("Text", quoted(decodeText(original)),
                                    quoted(decodeText(entry.bytes)))}, {bank.spec.id, recordId, -1, "description"});
        }
    }
    if (promotion_ && (reference || promotion_->modified())) {
        auto p = *promotion_;
        if (reference) { const auto &a = *reference->promotion_; p.originalFamilies=a.families; p.originalGates=a.gates; p.originalMasks=a.masks; p.originalFlags=a.flags; }
        const auto a = p.originalFamilies["assignments"].toArray(),
                   b = p.families["assignments"].toArray();
        auto familyName = [](const QJsonObject &catalog, int id) {
            for (const auto &entry : catalog["families"].toArray())
                if (entry.toObject()["id"].toInt() == id)
                    return Promotion::familyName(entry.toObject());
            return QString("Family %1").arg(id);
        };
        const QStringList gates{
            "STR", "VIT", "INT", "MEN", "AGI", "DEX", "Minimum alignment", "Maximum alignment"};
        for (int id = 1; id < p.classCount(); ++id) {
            QStringList lines;
            const bool added = id >= a.size();
            for (int f = 0; f < 8; ++f) {
                const int after = u8(p.gates, id * 8 + f);
                if (added || after != u8(p.originalGates, id * 8 + f))
                    lines << transition(
                        gates[f], added ? "Not present" : number(u8(p.originalGates, id * 8 + f)),
                        number(after));
            }
            if (added || a[id] != b[id])
                lines << transition(
                    "Family", added ? "Not present" : familyName(p.originalFamilies, a[id].toInt()),
                    familyName(p.families, b[id].toInt()));
            for (const auto &part :
                 {qMakePair(QString("Compatibility mask"), qMakePair(p.originalMasks, p.masks)),
                  qMakePair(QString("Equipment availability flags"),
                            qMakePair(p.originalFlags, p.flags))})
                if (added || part.second.first[id] != part.second.second[id])
                    lines << transition(
                        part.first,
                        added ? "Not present"
                              : QString("0x%1").arg(u8(part.second.first, id), 2, 16, QChar('0')),
                        QString("0x%1").arg(u8(part.second.second, id), 2, 16, QChar('0')));
            add("Class promotion", className(id), lines, {"classes", id, -1, "promotionEditor"});
        }
        const auto original = p.originalFamilies["families"].toArray();
        for (const auto &entry : p.families["families"].toArray()) {
            const auto family = entry.toObject();
            QJsonValue before;
            for (const auto &old : original)
                if (old.toObject()["id"] == family["id"])
                    before = old;
            QStringList lines;
            settings(lines, before, family);
            add("Class families", family["name"].toString(), lines, {"family", family["id"].toInt()});
        }
    }
    if (shops_ && (reference || shops_->modified())) {
        auto stockText = [&](const QVector<quint16> &items, bool consumable = false) {
            QStringList names;
            for (auto id : items)
                names << itemName(id, consumable);
            return names.isEmpty() ? QString("Empty") : names.join(", ");
        };
        for (int id = 1; id < shops_->size(); ++id) {
            const auto a = reference ? (id < reference->shops_->size() ? reference->shops_->stock(id) : ShopStock{}) : shops_->originalStock(id);
            const auto &b = shops_->stock(id);
            QStringList lines;
            if (a.equipment != b.equipment)
                lines << transition("Equipment", stockText(a.equipment), stockText(b.equipment));
            if (a.consumables != b.consumables)
                lines << transition("Consumables", stockText(a.consumables, true),
                                    stockText(b.consumables, true));
            add("Shops", QString("Shop %1 · %2").arg(id).arg(shops_->locations(id).join(", ")),
                lines, {"shops", id});
        }
    }
    if (consumableEffects_ && (reference || consumableEffects_->modified())) {
        const auto original = ConsumableEffects::load(baseRom).models();
        for (auto it = consumableEffects_->models().begin();
             it != consumableEffects_->models().end(); ++it) {
            QStringList lines;
            settings(lines, original[it.key()], it.value());
            add("Consumable effects", friendly(it.key()), lines);
        }
    }
    if (tools_ && (reference || tools_->modified())) {
        QStringList lines;
        auto before = ToolSettings::load(baseRom).json(), after = tools_->json();
        const auto affinityBefore = before.take("leaderAffinity").toObject()["rows"].toObject();
        const auto affinityAfter = after.take("leaderAffinity").toObject()["rows"].toObject();
        auto leaders = affinityBefore.keys();
        for (const auto &key : affinityAfter.keys()) if (!leaders.contains(key)) leaders << key;
        for (const auto &key : leaders) {
            QStringList bonuses;
            const auto a = affinityBefore[key].toArray(), b = affinityAfter[key].toArray();
            for (int group = 0; group < 4; ++group) {
                const int oldValue = group < a.size() ? a[group].toInt() : 0;
                const int newValue = group < b.size() ? b[group].toInt() : 0;
                if (oldValue != newValue) bonuses << transition(LeaderAffinity::groupName(group),number(oldValue),number(newValue));
            }
            add("Classes", className(key.toInt()) + " / Leader affinity", bonuses, {"classes",key.toInt(),-1,"section:Combat"});
        }
        const auto a = before.take("gameRules").toObject(), b = after.take("gameRules").toObject();
        for (const auto &rule : GameRules::definitions()) if (a[rule.key] != b[rule.key])
            add("Game rules", rule.label, {transition(rule.label, scalar(a[rule.key]), scalar(b[rule.key]))}, {"rule", -1, -1, rule.key});
        settings(lines, before, after);
        add("Optional tools", "Settings", lines);
    }
    if (encounters_ && (reference || encounters_->modified())) {
        const auto original = Encounters::load(baseRom, int(records(Table::Classes).size()));
        for (int p = 1; p <= Encounters::poolCount; ++p) {
            QStringList lines;
            for (int slot = 0; slot < encounters_->slotsPerPool(); ++slot)
                for (int side = 0; side < 2; ++side) {
                    const int a = (slot < original.slotsPerPool() ? original.classAt(p, slot, side) : 0),
                              b = encounters_->classAt(p, slot, side);
                    if (a != b)
                        lines << transition(
                            QString("Slot %1 / member %2").arg(slot + 1).arg(side + 1),
                            className(a), className(b));
                }
            add("Neutral encounters", encounters_->label(p), lines, {"encounters", p});
        }
        for (int t = 0; t < Encounters::terrainCount; ++t) {
            QStringList lines;
            if (original.rate(t) != encounters_->rate(t))
                lines << transition("Rate", number(original.rate(t)), number(encounters_->rate(t)));
            if (original.lookup(t) != encounters_->lookup(t))
                lines << transition("Pool slot", number(original.lookup(t)),
                                    number(encounters_->lookup(t)));
            add("Neutral encounters",
                Encounters::mapTerrainName(t), lines);
        }
        const auto a = original.json(), b = encounters_->json();
        for (const auto &key : {"globalRate", "rewards", "scenarioRates", "customSquads", "persuasion"}) {
            QStringList lines;
            settings(lines, a[key], b[key]);
            add("Neutral encounters", friendly(key), lines);
        }
    }
    if (scenarios_ && (reference || scenarios_->modified())) {
        for (const auto &s : scenarios_->entries()) {
            const auto &a = reference ? reference->scenarios_->scenario(s.key) : scenarios_->original(s.key);
            if (s.json() == a.json())
                continue;
            QStringList lines;
            if (a.layout.baseLevel() != s.layout.baseLevel())
                lines << transition("Base level", number(a.layout.baseLevel()),
                                    number(s.layout.baseLevel()));
            if (a.layout.header.mid(11, 2) != s.layout.header.mid(11, 2)) {
                auto time = [](const Eset &e) {
                    return QString("%1:%2").arg(e.startHour(), 2, 10, QLatin1Char('0'))
                                          .arg(e.startMinute(), 2, 10, QLatin1Char('0'));
                };
                add("Scenarios", QString("%1 · starting time").arg(s.name),
                    {transition("Starting time", time(a.layout), time(s.layout))}, {"scenarioSettings", s.key});
            }
            for(int boss=30;boss<=31;++boss) {
                auto reward=[&](const Scenario &scenario) {
                    for(const auto &d:scenario.layout.deployments) if((u8(d,0)&127)==boss)
                        return itemName(u16(d,11),u8(d,10)==0);
                    return QString("None");
                };
                if(reward(a)!=reward(s)) add("Scenarios",s.name+(boss==30 ? " · primary completion item" : " · fallback completion item"),
                    {transition("Reward",reward(a),reward(s))},{"scenarioSettings",s.key});
            }
            lines << rowChanges(a.squads, s.squads, "squad")
                  << rowChanges(a.layout.deployments, s.layout.deployments, "deployment")
                  << rowChanges(a.layout.nodes, s.layout.nodes, "movement node")
                  << rowChanges(a.layout.conditions, s.layout.conditions, "condition");
            if (lines.isEmpty() && a.layout.header.mid(11,2) == s.layout.header.mid(11,2))
                lines << "Updated scenario layout.";
            for(int squad=0;squad<s.squads.size();++squad) {
                if(squad<a.squads.size() && s.squads[squad]==a.squads[squad]) continue;
                const auto after=squadMembers(s.squads[squad]);
                const auto before=squad<a.squads.size() ? squadMembers(a.squads[squad]) : QVector<CustomMember>{};
                QStringList members;
                for(int m=0;m<std::max(before.size(),after.size());++m) {
                    auto describe=[&](const QVector<CustomMember> &values) {
                        if(m>=values.size()) return QString("None");
                        const auto &v=values[m];
                        return QString("%1, level offset %2, formation cell %3, %4 / %5")
                            .arg(className(v.classId)).arg(v.levelOffset).arg(v.cell+1).arg(itemName(v.equipment[0]),itemName(v.equipment[1]));
                    };
                    if(describe(before)!=describe(after)) members<<transition(QString("Member %1").arg(m+1),describe(before),describe(after));
                }
                add("Scenarios",QString("%1 · squad %2").arg(s.name).arg(squad+1),members,{"scenarios",s.key,squad});
            }
            add("Scenarios", QString("%1 (key %2)").arg(s.name).arg(s.key), lines, {"scenarios", s.key});
        }
    }
    if (world_ && (reference || world_->modified())) {
        const auto original = ScenarioWorld::load(baseRom);
        for (int id = 1; id <= ScenarioWorld::townCount; ++id) {
            const auto a = original.town(id), b = world_->town(id);
            if (a == b) continue;
            QStringList lines;
            if (original.townName(id) != world_->townName(id))
                lines << transition("Name", quoted(original.townName(id)), quoted(world_->townName(id)));
            for (const auto &field : {qMakePair(QString("Population"), 22),
                                      qMakePair(QString("Morale / objective"), 24),
                                      qMakePair(QString("Facilities"), 25),
                                      qMakePair(QString("Stronghold type"), 26),
                                      qMakePair(QString("Shop"), 27)}) {
                const int old = field.second == 22 ? u16(a, 22) : u8(a, field.second);
                const int value = field.second == 22 ? u16(b, 22) : u8(b, field.second);
                if (old != value) lines << transition(field.first, number(old), number(value));
            }
            add("Sites and treasures", world_->townName(id) + QString(" (site %1)").arg(id),
                lines, {"town", id});
        }
        const auto rows = world_->json()["scenarios"].toObject();
        for (auto it = rows.begin(); it != rows.end(); ++it) {
            const int key = it.key().toInt();
            const auto &a = reference ? reference->world_->scenario(key) : world_->original(key);
            const auto &b = world_->scenario(key);
            QStringList lines;
            if (a.sites != b.sites)
                lines << "Updated site allegiance.";
            if (a.treasures != b.treasures)
                lines << transition("Treasure entries",
                                    number(ScenarioWorld::treasureCount(a.treasures)),
                                    number(ScenarioWorld::treasureCount(b.treasures)))
                      << "Updated treasure placement or contents.";
            add("Sites and treasures", "Scenario key " + it.key(), lines, {"map", key});
        }
    }
    if(!reference && mapEdits_ && mapEdits_->modified()){
        const auto maps=mapEdits_->json()["maps"].toObject();
        for(auto it=maps.begin();it!=maps.end();++it){
            const auto row=it->toObject();const auto paint=QByteArray::fromBase64(row["paint"].toString().toLatin1());int cells=0;for(unsigned char c:paint)cells+=c!=255;
            QStringList lines{QString("Painted %1 terrain cells; moved %2 towns or strongholds. "
                                      "Shared missions use the same map.")
                                  .arg(cells)
                                  .arg(row["sites"].toObject().size())};
            if (!row["city"].toArray().isEmpty())
                lines << QString("Placed %1 city scenery pieces.").arg(row["city"].toArray().size());
            if (row.contains("boundary")) {
                const auto edge = row["boundary"].toArray();
                lines << QString("Playable boundary: left %1, top %2, right %3, bottom %4.")
                             .arg(edge[0].toInt())
                             .arg(edge[1].toInt())
                             .arg(edge[2].toInt())
                             .arg(edge[3].toInt());
            }
            add("Maps", "Battlefield " + it.key(), lines, {"battlefield", it.key().toInt()});
        }
    }
    if (scenePrograms_ && (reference || scenePrograms_->modified())) {
        if (!reference) for (auto it = scenePrograms_->plan().begin(); it != scenePrograms_->plan().end(); ++it) {
            const auto before = scenePrograms_->resource(it.key()).program.commands.size();
            const auto after = scenePrograms_->program(it.key()).commands.size();
            add("Cutscenes",
                "Scene " + QString::number(it.key(), 16).rightJustified(8, '0').toUpper(),
                {transition("Commands", number(int(before)), number(int(after))),
                 "Updated scene commands."}, {"scene", int(it.key())});
        }
        const auto original = ScenePrograms::load(baseRom).authoredScenes();
        auto ids = original.keys();
        for (const auto &id : scenePrograms_->authoredScenes().keys())
            if (!ids.contains(id))
                ids << id;
        for (const auto &id : ids) {
            const auto &current = scenePrograms_->authoredScenes();
            if (!original.contains(id))
                add("Cutscenes", current[id].name,
                    {QString("Added scene with %1 commands.")
                         .arg(current[id].current.commands.size())}, {"scene", -1, -1, id});
            else if (!current.contains(id))
                add("Cutscenes", original[id].name, {"Removed authored scene."});
            else {
                QStringList lines;
                if (original[id].name != current[id].name)
                    lines << transition("Name", quoted(original[id].name),
                                        quoted(current[id].name));
                if (original[id].current.source != current[id].current.source)
                    lines << "Updated scene commands.";
                add("Cutscenes", current[id].name, lines, {"scene", -1, -1, id});
            }
        }
    }
    if (sceneEvents_ && (reference || sceneEvents_->modified())) {
        const auto original = SceneEvents::load(baseRom);
        for (const auto &site : SceneEvents::sites())
            if (original.target(site.id) != sceneEvents_->target(site.id))
                add("Cutscene events", site.mission + " · " + site.name + " / " + site.role,
                    {transition("Scene selector", number(original.target(site.id)),
                                number(sceneEvents_->target(site.id)))});
        auto rows = SceneEvents::flagRows(sceneEvents_->flagEdits());
        rows.unite(SceneEvents::flagRows(original.flagEdits()));
        for (int row : rows) {
            const auto controls = EventScript::flagSteps(SceneEvents::retailRow(baseRom, row));
            const auto before = original.flagEdits()[QString::number(row)].toObject();
            const auto after = sceneEvents_->flagEdits()[QString::number(row)].toObject();
            auto offsets = before.keys();
            for (const auto &offset : after.keys()) if (!offsets.contains(offset)) offsets << offset;
            for (const auto &offset : offsets)
                if (before[offset] != after[offset]) {
                    const auto step = controls.value(offset.toInt());
                    auto describe = [&](const QJsonObject &edits) {
                        const auto edit = edits[offset].toObject();
                        const int flag = edit.isEmpty() ? step.flag : edit["flag"].toInt();
                        const bool value = edit.isEmpty() ? step.value : edit["value"].toBool();
                        return QString("%1 %2%3").arg(step.check ? "Check" : value ? "Set" : "Clear",
                            EventScript::flagName(flag), step.check ? value ? " is ON" : " is OFF" : "");
                    };
                    add("Scenario events", QString("Event row %1 · instruction @%2").arg(row).arg(offset.toInt(), 4, 16, QChar('0')),
                        {describe(before) + " → " + describe(after)}, {"scenario-event", row, offset.toInt()});
                }
        }
    }
    if (!reference && dialogues_ && dialogues_->modified()) {
        const auto archives = dialogues_->json()["archives"].toObject();
        for (auto it = archives.begin(); it != archives.end(); ++it) {
            const int identity = it.key().toInt(), archive = identity & 65535;
            const auto table =
                identity & 65536 ? Dialogues::Table::Alternate : Dialogues::Table::Primary;
            const QString prefix = QString("%1 archive %2")
                                       .arg(identity & 65536 ? "Alternate" : "Primary")
                                       .arg(archive);
            const auto edits = it.value().toObject()["entries"].toObject();
            for (auto entry = edits.begin(); entry != edits.end(); ++entry)
                add("Dialogue", prefix + " / entry " + entry.key(),
                    { "Updated dialogue text and controls." }, {"dialogue", identity, entry.key().toInt()});
            const auto added = it.value().toObject()["added"].toArray();
            const int first = dialogues_->original(table, archive).entryCount();
            for (int i = 0; i < added.size(); ++i)
                add("Dialogue", prefix + QString(" / entry %1").arg(first + i),
                    { "Added dialogue entry." }, {"dialogue", identity, first+i});
        }
    }
    if (!reference && categoryIcons_ && categoryIcons_->modified())
        add("UI and Icons", "Class category icons",
            {"Edited icon artwork, custom icons or family tab assignments."});
    if (!reference && icons_ && icons_->modified())
        for (const auto &id : icons_->json()["icons"].toObject().keys())
            add("Item icons", itemName(id.toInt() & 0x1ff, (id.toInt() & 0x200) != 0),
                {"Edited icon pixels."}, {"icon", id.toInt()});
    if (!reference && avatars_ && avatars_->modified())
        for (const auto &value : avatars_->json()["portraits"].toArray()) {
            const auto row = value.toObject();
            add("Portraits", className(row["classId"].toInt()),
                {QString("Edited portrait %1.").arg(row["selector"].toInt() + 1)}, {"portrait", row["classId"].toInt(), row["selector"].toInt()});
        }
    if (!reference && army_ && army_->modified()) {
        for (int atlas = 0; atlas < ArmyArtwork::specs().size(); ++atlas)
            for (int model = 0; model < ArmyArtwork::specs()[atlas].capacity; ++model)
                if (army_->modified(atlas, model))
                    add("Army sprites",
                        ArmyArtwork::specs()[atlas].label + QString(" / model %1").arg(model),
                        {"Edited sprite pixels."}, {"army", atlas, model});
        for (auto it = army_->routes().begin(); it != army_->routes().end(); ++it)
            add("Army sprites", className(it.key()),
                {QString("Assigned lane %1, model %2.").arg(it->lane).arg(it->model)}, {"armyClass", it.key()});
    }
    if (!reference && combat_ && combat_->modified()) {
        const auto bindings = combat_->json()["bindings"].toObject();
        for (auto it = bindings.begin(); it != bindings.end(); ++it)
            for (const auto &child : it.value().toObject()["children"].toObject().keys())
                add("Combat sprites", QString("Sprite %1 / child %2").arg(it.key(), child),
                    {"Edited sprite color or opacity."}, {"combatSprite", -1, child.toInt(), it.key()});
    }
    if (animationRoutes_ && (reference || animationRoutes_->modified())) {
        const auto original = AnimationRoutes::load(baseRom);
        const auto baseline =
            animationSequences_ ? animationSequences_->baseRoutes(original) : original;
        for (int id = 0; id < animationRoutes_->rowCount(); ++id)
            if ((reference || animationRoutes_->edits().contains(id)) &&
                (id >= baseline.rowCount() || animationRoutes_->entry(id) != baseline.entry(id)))
                add("Animation assignments", className(id),
                    {"Updated body routes, speeds, attack selectors or visual policies."}, {"animation", id});
    }
    if (animationOverrides_ && (reference || animationOverrides_->modified())) {
        const auto originalRoutes = AnimationRoutes::load(baseRom);
        const auto original = AnimationOverrides::load(baseRom, ClassData::load(baseRom), originalRoutes);
        QStringList lines;
        settings(lines, original.entries(), animationOverrides_->entries(), "Attack override");
        add("Animation assignments", "Normal / Blocked attacks", lines);
    }
    if (animationSequences_ && (reference || animationSequences_->modified())) {
        const auto original = AnimationSequences::load(baseRom).json()["groups"].toArray();
        const auto current = animationSequences_->json()["groups"].toArray();
        QMap<int, QJsonObject> before, after;
        for (const auto &row : original) {
            const auto o = row.toObject();
            before[o["classId"].toInt() * 4 + o["bodyFlags"].toInt()] = o;
        }
        for (const auto &row : current) {
            const auto o = row.toObject();
            after[o["classId"].toInt() * 4 + o["bodyFlags"].toInt()] = o;
        }
        auto keys = before.keys();
        for (int key : after.keys())
            if (!keys.contains(key))
                keys << key;
        for (int key : keys)
            if (before.value(key) != after.value(key))
                add("Private animations", className(key / 4) + QString(" / body %1").arg(key % 4),
                    {before.contains(key) ? after.contains(key)
                                                ? "Updated private sequence, frames or artwork."
                                                : "Removed private animation."
                                          : "Created private animation."}, {"combatAnimation", key / 4, key % 4});
    }
    if(training_) for(int i=0;i<training_->entries().size();++i) {
        const auto &row=training_->entries()[i];
        auto before=training_->original(i);
        if(reference) for(const auto &candidate:reference->training_->entries())
            if(candidate.node==row.node && candidate.slot==row.slot) {before=candidate;break;}
        if(row.json()==before.json()) continue;
        QStringList lines;
        if(row.price!=before.price) lines<<transition("Price factor",number(before.price),number(row.price));
        const auto oldMembers=squadMembers(before.squad),newMembers=squadMembers(row.squad);
        for(int m=0;m<std::max(oldMembers.size(),newMembers.size());++m) {
            auto describe=[&](const QVector<CustomMember> &members) {
                if(m>=members.size()) return QString("None");
                const auto &v=members[m];
                return QString("%1, level offset %2, cell %3, equipment %4 / %5").arg(className(v.classId)).arg(v.levelOffset).arg(v.cell+1).arg(v.equipment[0]).arg(v.equipment[1]);
            };
            if(describe(oldMembers)!=describe(newMembers)) lines<<transition(QString("Member %1").arg(m+1),describe(oldMembers),describe(newMembers));
        }
        add("Training",row.name+QString(" / opponent %1").arg(row.slot+1),lines,{"training",row.node,row.slot});
    }
    if (reference) {
        const auto &old = *reference;
        auto art = [&](const QString &area, const QString &subject, const QByteArray &a, const QByteArray &b, EditLocation target) {
            if (a != b) add(area, subject, {a.isEmpty() ? "Added artwork." : "Replaced artwork; open this entry to inspect its pixels."}, target);
        };
        for (int id : categoryIcons_->iconIds())
            art("UI and Icons", categoryIcons_->label(id), old.categoryIcons_->iconIds().contains(id) ? old.categoryIcons_->indices(id) : QByteArray{}, categoryIcons_->indices(id), {"categoryIcon", id});
        QStringList categories;
        const auto oldCategories=old.categoryIcons_->json(), newCategories=categoryIcons_->json();
        settings(categories,oldCategories["families"],newCategories["families"],"Family icon");
        add("UI and Icons","Family tab assignments",categories);
        for(int pack=0;pack<2;++pack) for(int id=1;id<=(pack ? IconArtwork::specs()[1].count : icons_->equipmentCount());++id) {
            const int key=id|(pack ? 0x200 : 0);
            art("Item icons",itemName(id,pack),!pack && id>old.icons_->equipmentCount() ? QByteArray{} : old.icons_->words(key),icons_->words(key),{"icon",key});
        }
        const auto classes=classData(), oldClasses=old.classData();
        for(int id=0;id<classes.entries().size();++id) for(int selector=0;selector<2;++selector)
            art("Portraits",className(id)+QString(" / portrait %1").arg(selector+1),id<oldClasses.entries().size() ? old.avatars_->words(oldClasses,id,selector) : QByteArray{},avatars_->words(classes,id,selector),{"portrait",id,selector});
        for(int atlas=0;atlas<ArmyArtwork::specs().size();++atlas) for(int model=0;model<ArmyArtwork::specs()[atlas].capacity;++model) if(army_->present(atlas,model))
            art("Army sprites",ArmyArtwork::specs()[atlas].label+QString(" / model %1").arg(model),old.army_->present(atlas,model) ? old.army_->indices(atlas,model) : QByteArray{},army_->indices(atlas,model),{"army",atlas,model});
        for(int id=0;id<classes.entries().size();++id) {
            const auto &b=classes.entries()[id];
            if(id<oldClasses.entries().size()) {
                const auto &a=oldClasses.entries()[id];
                if(ArmyArtwork::classLane(a)!=ArmyArtwork::classLane(b) || ArmyArtwork::classModel(a)!=ArmyArtwork::classModel(b))
                    add("Army sprites",className(id),{transition("Sprite",QString("lane %1, model %2").arg(ArmyArtwork::classLane(a)).arg(ArmyArtwork::classModel(a)),QString("lane %1, model %2").arg(ArmyArtwork::classLane(b)).arg(ArmyArtwork::classModel(b)))},{"armyClass",id});
            }
        }
        const auto editedBindings=combat_->json()["bindings"].toObject();
        for(const auto &binding:combat_->bindings()) {
            const bool present=old.combat_->bindings().contains(binding.id);
            // Some native members are not editable SpriteObjects. Compare their decoded
            // bytes first, so unchanged opaque members never fail the review.
            auto raw = [](const Rom &rom, quint32 key) {
                const auto stored=fieldResource(rom,key); const auto size=u32(stored,0);
                if(size>16*1024*1024) throw Error("Sprite comparison exceeds its decode budget");
                return decodeLzss(stored.mid(4),size);
            };
            const bool edited=editedBindings.contains(binding.id);
            if(present && !edited && binding.palette==old.combat_->binding(binding.id).palette &&
               raw(rom_,binding.resourceKey)==raw(baseRom,old.combat_->binding(binding.id).resourceKey)) continue;
            const auto current=combat_->current(binding.id);
            const auto prior=present ? old.combat_->current(binding.id) : SpriteObject{};
            if(present && current.source()==prior.source() && binding.palette==old.combat_->binding(binding.id).palette) continue;
            const auto count=present ? prior.children().size() : 0;
            for(int child=0;child<current.children().size();++child) {
                const auto image=combat_->image(binding.id,child);
                if(child>=count || image!=old.combat_->image(binding.id,child))
                    add("Combat sprites",QString("Sprite %1 / child %2").arg(binding.id).arg(child),{"Replaced sprite artwork."},{"combatSprite",-1,child,binding.id});
            }
        }
        QSet<int> maps;
        for(const auto &scenario:scenarios_->entries()) {
            const int key=scenario.key, modelId=u8(rom_.bytes(),0x654a0+key*9+3);
            if(key==54 || modelId<4 || modelId>44) continue;
            const auto &src=mapEdits_->source(key);
            if(maps.contains(src.modelId)) continue; maps.insert(src.modelId);
            const auto model=mapEdits_->model(key), original=old.mapEdits_->model(key);
            const auto terrain=mapEdits_->fineTerrain(key), oldTerrain=old.mapEdits_->fineTerrain(key);
            QStringList lines; int changed=0;
            for(int i=0;i<std::min(terrain.size(),oldTerrain.size());++i) changed+=terrain[i]!=oldTerrain[i];
            if(changed) lines<<QString("Terrain: %1 cells differ from the comparison map.").arg(changed);
            if(model!=original) lines<<"Updated mountain shape, terrain artwork or scenery.";
            if(mapEdits_->points(key)!=old.mapEdits_->points(key)) lines<<"Moved town or stronghold positions.";
            if(mapEdits_->boundary(key)!=old.mapEdits_->boundary(key)) lines<<"Changed playable boundary.";
            add("Maps",QString("Battlefield %1").arg(src.modelId),lines,{"map",key});
        }
        for(const auto &scene:scenePrograms_->original().entries()) {
            quint32 oldKey=scene.key;
            if(!scene.selectors.isEmpty()) {
                const int selector=scene.selectors.first();
                const auto &directory=old.scenePrograms_->original().selectors();
                if(selector<directory.size()) oldKey=directory[selector];
            }
            if(!old.scenePrograms_->original().entries().contains(oldKey)) continue;
            const auto a=old.scenePrograms_->program(oldKey), b=scenePrograms_->program(scene.key);
            if(a.source!=b.source) add("Cutscenes",scenePrograms_->displayName(scene.key),
                {transition("Commands",number(a.commands.size()),number(b.commands.size())),"Updated scene commands."},{"scene",int(scene.key)});
        }
        for(auto table:{Dialogues::Table::Primary,Dialogues::Table::Alternate}) {
            const auto a=DialogueArchive::directory(baseRom,table), b=DialogueArchive::directory(rom_,table);
            for(int archive=0;archive<b.size();++archive) {
                if(!b[archive] || b[archive]>=0x10000000 || b[archive]%2) continue;
                const int before=archive<a.size() && a[archive] && a[archive]<0x10000000 && !(a[archive]%2) ? old.dialogues_->entryCount(table,archive) : 0;
                for(int entry=0;entry<dialogues_->entryCount(table,archive);++entry) {
                    const auto text=dialogues_->entry(table,archive,entry), original=entry<before ? old.dialogues_->entry(table,archive,entry) : QByteArray{};
                    if(entry>=before || text!=original) add("Dialogue",QString("%1 archive %2 / entry %3").arg(table==Dialogues::Table::Primary ? "Primary" : "Alternate").arg(archive).arg(entry),
                        {transition("Text",quoted(decodeText(original)),quoted(decodeText(text)))},{"dialogue",int(Dialogues::identity(table,archive)),entry});
                }
            }
        }
    }
    for (const auto &asset : sprites_.assets())
        add("Sprite library", asset.name,
            {QString("%1 × %2; %3 frames. Project asset; game assignments are listed separately.")
                 .arg(asset.size.width())
                 .arg(asset.size.height())
                 .arg(asset.frames.size())}, {"sprites", -1, -1, asset.id});
    for (const auto &section : pendingLegacySections())
        add("Imported project data", friendly(section),
            {"Retained browser data awaiting native conversion; not a verified ROM edit."});
    return report;
}
} // namespace lordly
