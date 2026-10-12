#include "engine/project_references.h"
#include "engine/actor_art.h"
#include "engine/squad_members.h"
#include <QJsonArray>
#include <functional>

namespace lordly {
QVector<ProjectReference> projectReferences(Project p) {
    QVector<ProjectReference> uses;
    if (p.rom().revision() == 0) {
        p.loadShops(); p.loadTraining(); p.loadScenarios(); p.loadWorld(); p.loadEncounters(); p.loadAnimationRoutes();
    }
    const QMap<QString, int> counts{{"classes", int(p.records(Table::Classes).size())},
        {"items", int(p.records(Table::Items).size())},
        {"consumables", int(p.records(Table::Consumables).size())},
        {"shops", p.shops() ? p.shops()->size() : 0}};
    auto add = [&](QString kind, int id, QString description, EditLocation location, bool changed) {
        if (id <= 0) return;
        uses << ProjectReference{kind, id, description, location, id < counts.value(kind), changed};
    };
    auto reward = [&](int value, const QString &label, EditLocation location, bool changed) {
        add(value & 0x8000 ? "items" : "consumables", value & 0x7fff, label, location, changed);
    };
    for (const auto &row : p.records(Table::Classes)) {
        if (row.id == 0) continue; // The None placeholder contains sentinel values, not references.
        for (const auto &field : fields(Table::Classes)) {
            if (field.choices != "ITEM_NAMES" && field.choices != "CLASS_NAMES") continue;
            const int value = field.width == 2 ? u16(row.bytes, field.offset) : u8(row.bytes, field.offset);
            const int before = row.romOffset < 0 ? -1 : field.width == 2 ? u16(row.original, field.offset) : u8(row.original, field.offset);
            add(field.choices == "ITEM_NAMES" ? "items" : "classes", value,
                row.name + " / " + field.label, {"classes", row.id, -1, field.key}, value != before);
        }
    }
    if (p.shops()) for (int shop = 1; shop < p.shops()->size(); ++shop) {
        const auto &stock = p.shops()->stock(shop), &original = p.shops()->originalStock(shop);
        for (int item : stock.equipment)
            add("items", item, QString("Shop %1 / equipment stock").arg(shop), {"shops", shop}, !original.equipment.contains(item));
        for (int item : stock.consumables)
            add("consumables", item, QString("Shop %1 / consumable stock").arg(shop), {"shops", shop}, !original.consumables.contains(item));
    }
    if (p.scenarios()) for (const auto &scenario : p.scenarios()->entries()) {
        const auto &original = p.scenarios()->original(scenario.key);
        for (int squad = 0; squad < scenario.squads.size(); ++squad) {
            const bool changed = squad >= original.squads.size() || scenario.squads[squad] != original.squads[squad];
            const auto members = squadMembers(scenario.squads[squad]);
            const auto location = EditLocation{"scenarios", scenario.key, squad};
            for (int i = 0; i < members.size(); ++i) {
                const auto &member = members[i];
                const auto label = QString("%1 / squad %2 / member %3").arg(scenario.name).arg(squad + 1).arg(i + 1);
                add("classes", member.classId, label, location, changed);
                for (int slot = 0; slot < 2; ++slot)
                    add("items", member.equipment[slot], label + QString(" / equipment %1").arg(slot + 1), location, changed);
            }
            const auto &deployment = scenario.layout.deployments[squad];
            const bool deploymentChanged = squad >= original.layout.deployments.size() || deployment != original.layout.deployments[squad];
            reward(u16(deployment, 11) | (u8(deployment, 10) ? 0x8000 : 0),
                   QString("%1 / squad %2 / dropped item").arg(scenario.name).arg(squad + 1), location, deploymentChanged);
        }
    }
    if(p.training()) for(int i=0;i<p.training()->entries().size();++i) {
        const auto &row=p.training()->entries()[i];
        const bool changed=row.squad!=p.training()->original(i).squad;
        const auto members=squadMembers(row.squad);
        for(int m=0;m<members.size();++m) {
            const auto label=QString("%1 / training opponent %2 / member %3").arg(row.name).arg(row.slot+1).arg(m+1);
            const EditLocation where{"training",row.node,row.slot};
            add("classes",members[m].classId,label,where,changed);
            for(int item:members[m].equipment) add("items",item,label,where,changed);
        }
    }
    if (p.world()) {
        const auto original = ScenarioWorld::load(p.rom());
        for (int town = 1; town <= ScenarioWorld::townCount; ++town) {
            const auto record = p.world()->town(town), before = original.town(town);
            if (u8(record, 25) & 1)
                add("shops", u8(record, 27), p.world()->townName(town) + " / shop", {"town", town},
                    u8(record, 27) != u8(before, 27) || !(u8(before, 25) & 1));
        }
        for (int key = 1; key <= 64; ++key) {
            const auto &data = p.world()->scenario(key).treasures;
            const auto &before = p.world()->original(key).treasures;
            for (int row = 0; row < ScenarioWorld::treasureCount(data); ++row) {
                const auto item = ScenarioWorld::treasureRecord(data, row);
                bool changed = true;
                for (int old = 0; old < ScenarioWorld::treasureCount(before); ++old)
                    if (ScenarioWorld::treasureRecord(before, old) == item) changed = false;
                add(u8(item, 3) == 1 ? "items" : "consumables", u16(item, 4),
                    QString("Scenario %1 / treasure %2").arg(key).arg(row + 1), {"map", key, row, "treasure"}, changed);
            }
        }
    }
    if (p.encounters()) {
        const auto original = Encounters::load(p.rom(), int(p.records(Table::Classes).size()));
        for (int pool = 1; pool <= Encounters::poolCount; ++pool)
            for (int slot = 0; slot < p.encounters()->slotsPerPool(); ++slot)
                for (int side = 0; side < 2; ++side) {
                    const int value = p.encounters()->classAt(pool, slot, side);
                    add("classes", value, p.encounters()->label(pool) + QString(" / encounter slot %1 / member %2").arg(slot + 1).arg(side + 1),
                        {"encounters", pool}, slot >= original.slotsPerPool() || value != original.classAt(pool, slot, side));
                }
        for (int cls = 0; cls < CreatureRewards::count; ++cls)
            for (int slot = 0; slot < 3; ++slot) {
                const auto &row = p.encounters()->rewards().row(cls);
                if (!row.weights[slot]) continue;
                reward(row.outcomes[slot], QString("Class %1 / encounter reward %2").arg(cls).arg(slot + 1),
                       {"encounterReward", cls}, row != original.rewards().row(cls));
            }
        for (const auto &squad : p.encounters()->customSquads().profiles()) {
            const auto label = QString("Scenario %1 / custom encounter %2").arg(squad.scenarioKey).arg(squad.terrainSlot + 1);
            const auto location = EditLocation{"customEncounter", squad.scenarioKey, squad.terrainSlot};
            for (int i = 0; i < squad.members.size(); ++i) {
                const auto &member = squad.members[i];
                add("classes", member.classId, label + QString(" / member %1").arg(i + 1), location, true);
                for (int item : member.equipment) add("items", item, label + QString(" / member %1 equipment").arg(i + 1), location, true);
            }
            for (int slot = 0; slot < 3; ++slot) if (squad.rewards.weights[slot])
                reward(squad.rewards.outcomes[slot], label + QString(" / reward %1").arg(slot + 1), location, true);
        }
    }
    if (p.animationRoutes()) for (const auto &cls : p.records(Table::Classes)) {
        const auto &routes = *p.animationRoutes();
        if (cls.id >= routes.rowCount()) continue;
        const int item = routes.entry(cls.id).equipmentArt;
        add("items", item, cls.name + " / fixed equipment artwork", {"animation", cls.id}, routes.edits().contains(cls.id));
    }
    return uses;
}
QVector<ProjectChange> findProjectUses(Project p, const QString &kind, int id, const QString &key) {
    QVector<ProjectChange> result;
    if (kind == "sprites") {
        const auto &asset = p.sprites().asset(key);
        if (asset.provenance.contains("classId"))
            result << ProjectChange{"Original artwork source", asset.name,
                {"Imported from this class. Art Library assignments copy artwork; they are not live links."},
                {"combatAnimation", asset.provenance["classId"].toInt(), asset.provenance["bodyFlags"].toInt()}};
        return result;
    }
    if (kind == "armySprite") {
        p.loadArmyArtwork(); bool valid=false; const int model=key.toInt(&valid);
        if(!valid) throw Error("Choose an army sprite model.");
        for(int cls:p.armyArtwork()->consumers(p.classData(),id,model))
            result << ProjectChange{"Army artwork",p.record(Table::Classes,cls).name,
                {QString("Uses %1 / model %2. Shared pixel edits affect these classes until a class receives an independent copy.").arg(ArmyArtwork::specs()[id].label).arg(model)}, {"armyClass",cls}};
        return result;
    }
    if (kind == "combatSprite") {
        p.loadCombatArtwork();
        const auto &binding = p.combatArtwork()->binding(key);
        const auto body = p.bodyAnimationData();
        for (const auto &cls : p.records(Table::Classes)) {
            const int link = u8(cls.bytes, 69), owner = link ? link : cls.id;
            if (owner >= p.records(Table::Classes).size()) continue;
            for (int flags = 0; flags < 4; ++flags) {
                const int handle = body->handle(cls.id, owner, flags >> 1, flags & 1);
                if (!binding.handles.contains(handle)) continue;
                const bool independent = body->descriptor(cls.id, owner, flags >> 1, flags & 1) != nullptr;
                result << ProjectChange{"Combat artwork", cls.name,
                    {QString("%1 / %2 artwork%3").arg(flags & 1 ? "Enemy" : "Player", flags & 2 ? "Alternate" : "Base",
                        independent ? " (independent animation copy)" : "")}, {"combatAnimation", cls.id, flags}};
            }
        }
        return result;
    }
    for (const auto &use : projectReferences(std::move(p)))
        if (use.kind == kind && use.id == id)
            result << ProjectChange{"Used by", use.description, {QString("References %1 ID %2").arg(kind).arg(id)}, use.location};
    return result;
}
QVector<ExportIssue> exportIssues(Project p) {
    QVector<ExportIssue> issues;
    if (p.rom().revision() == 0) { p.loadScenarios(); p.loadEncounters(); }
    bool brokenItem = false;
    for (const auto &ref : projectReferences(p))
        if (!ref.valid)
        {
            brokenItem |= ref.changed && ref.kind == "items";
            const QMap<QString, QString> nouns{{"items", "item"}, {"classes", "class"},
                                               {"consumables", "consumable"}, {"shops", "shop"}};
            issues << ExportIssue{ref.description + QString(" points to missing %1 ID %2.").arg(nouns.value(ref.kind, ref.kind)).arg(ref.id), ref.location, ref.changed};
        }
    const auto check = [&](const EditLocation &location, const std::function<void()> &validate) {
        try { validate(); } catch (const std::exception &e) { issues << ExportIssue{QString::fromUtf8(e.what()), location, true}; }
    };
    if (!brokenItem) check({"items"}, [&] { p.validateItemReferences(); });
    check({"training"},[&] { if(p.training()) p.training()->validate(p.classData(),int(p.records(Table::Items).size())); });
    check({"scenarios"}, [&] { if (p.scenarios()) p.scenarios()->validateForExport(p.classData()); });
    check({"Neutral encounters"}, [&] { if (p.encounters()) p.encounters()->customSquads().validateFormations(p.classData()); });
    check({"Animation assignments"}, [&] { p.validateAnimationAssignments(); });
    check({"Cutscenes"}, [&] { if (p.scenePrograms()) p.scenePrograms()->validateReferences(p.rom(), p.dialogues() ? &*p.dialogues() : nullptr); });
    return issues;
}
} // namespace lordly
