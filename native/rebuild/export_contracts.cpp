#include "rebuild/export_contracts.h"
#include "engine/class_data.h"
#include "engine/mission_tables.h"
#include "engine/battlefield_clone.h"
#include "engine/creature_rewards.h"
#include "engine/encounter_rate.h"
#include "engine/shop_runtime.h"
#include "engine/scenario_member_runtime.h"
#include "engine/icon_artwork.h"
#include "engine/army_artwork.h"
#include "engine/animation_routes.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>

namespace lordly {
namespace {
const QJsonObject &catalog() {
    static const auto value = [] {
        QFile file(":/generated/export-owners.json");
        if (!file.open(QIODevice::ReadOnly)) throw Error("Missing export ownership catalog");
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        if (root["claims"].toArray().isEmpty()) throw Error("Empty export ownership catalog");
        return root;
    }();
    return value;
}
// These are ownership namespaces, not game IDs. Integrated builders retain their
// existing internal checks; separate top-level writers get separate plan steps.
const QMap<QString, QStringList> groups{
    {"class-runtime", {"classes/relocations.json", "items/relocations.json", "classes/save-owners.json",
        "classes/text-owners.json", "items/text-owners.json", "combat-affinity", "consumable-trees",
        "custom-story-flags", "encounters/custom-owners/catalog.json"}},
    {"descriptions", {"classes/text-owners.json", "items/text-owners.json", "location-descriptions"}},
    {"experience", {"tools/experience.json"}},
    {"shops", {"shops/owners.json"}},
    {"animation-overrides", {"art/animation-overrides.json"}},
    {"chaos-counter", {"tools/chaos.json"}},
    {"display-tools", {"display-luck", "display-alignment"}},
    {"high-attack", {"tools/high-attack.json"}},
    {"game-rules", {"tools/game-rules.json", "tools/status-effects.json"}},
    {"mission-tables", {"mission-tables"}},
    {"army-capacity", {"scenarios/capacity/owners.json", "capacity-references", "scenarios/capacity-bounds.json",
        "scenarios/capacity-save-owners.json", "capacity-extra", "capacity-predicates"}},
    {"attack-references", {"attacks/hooks.json", "attacks/relocations.json", "attacks/effect-targets.json"}},
    {"world-map-runtime", {"campaign"}},
    {"event-gates", {"retail-event-gates"}},
    {"campaign-start", {"new-campaign"}},
    {"mission-runtime", {"authored-missions"}},
    {"battlefield-loaders", {"scenario-battlefields"}},
    {"events", {"event-directory"}},
    {"cursor-boundaries", {"cursor-boundary"}},
    {"movement", {"movement"}}
};
// Only explicitly named, ordered pairs use catalogued composition ranges. The
// symmetric static audit alone never grants permission to a new writer.
void composeCatalog(ExportPlan &plan, const QString &later, const QString &earlier) {
    ExportPlan::Ranges ranges;
    const auto policies = catalog()["sharing"].toObject();
    for (const auto &a : groups.value(later)) for (const auto &b : groups.value(earlier)) {
        QStringList pair{a,b}; pair.sort();
        for (const auto &entry : policies[pair.join(" + ")].toObject()["ranges"].toArray()) {
            const auto range = entry.toArray();
            ranges.append({qsizetype(range[0].toString().toULongLong(nullptr,16)),
                           qsizetype(range[1].toString().toULongLong(nullptr,16))});
        }
    }
    if (!ranges.isEmpty()) plan.compose(later, earlier, ranges);
}
}
ExportPlan::Ranges exportOwners(const QString &step) {
    ExportPlan::Ranges result;
    if (step == "scenarios") {
        const auto member = scenarioMemberOwner();
        const auto start = qsizetype(member["start"].toInteger());
        result.append({start, start + member["size"].toInt()});
    }
    if (step == "encounters" || step == "custom-rewards")
        result.append({CreatureRewards::ownerOffset, CreatureRewards::ownerOffset + CreatureRewards::ownerSize});
    if (step == "encounters")
        result.append({EncounterRate::romOffset, EncounterRate::romOffset + EncounterRate::codeSize});
    const auto names = groups.value(step);
    for (const auto &entry : catalog()["claims"].toArray()) {
        const auto owner = entry.toObject();
        if (names.contains(owner["group"].toString()))
            result.append({qsizetype(owner["start"].toInteger()), qsizetype(owner["end"].toInteger())});
    }
    return result;
}
void configureExportComposition(ExportPlan &plan) {
    // These installers refresh checksums in the class descriptor after linking
    // already allocated native data/code. Pointer/count fields remain protected.
    constexpr auto header = ClassPlacement::descriptorRomOffset;
    plan.compose("mission-tables", "class-runtime", {{header+52,header+56}, {header+112,header+116}, {header+124,header+128}});
    plan.compose("army-capacity", "class-runtime", {{header+92,header+96}, {header+124,header+128}});
    plan.compose("army-capacity", "mission-tables", {{header+124,header+128}});
    // Movement records the final pre-install hook words after capacity and
    // mission linking, then refreshes the enclosing allocation's checksums.
    for (const auto &earlier : {"class-runtime", "mission-tables", "army-capacity"})
        plan.compose("movement", earlier, {{header+52,header+56}, {header+124,header+128}});
    constexpr auto missions = MissionTables::descriptorOffset;
    // Map edits own table rows. Linking sets only their runtime pointer and the
    // descriptor checksum, after all rows have been written.
    plan.compose("mission-tables", "maps", {{missions+16,missions+20}, {missions+60,missions+64}});
    constexpr auto fields = BattlefieldClone::descriptorOffset;
    plan.compose("battlefield-loaders", "maps", {{fields+40,fields+44}, {fields+60,fields+64}});
    // Custom encounters rebuild the same reward/dispatcher owners using the
    // configured table addresses plus their newly linked custom-squad services.
    const ExportPlan::Ranges rewardOwner{{CreatureRewards::ownerOffset, CreatureRewards::ownerOffset + CreatureRewards::ownerSize}};
    plan.compose("class-runtime", "custom-rewards", rewardOwner);
    plan.compose("class-runtime", "encounters", rewardOwner);
    // Movement composes only the original table-copy/storm block, preserving
    // configured rewards and artwork elsewhere in this shared setup owner.
    for (const auto &earlier : {"class-runtime", "encounters", "custom-rewards"})
        plan.compose("movement", earlier, {{0x102db0,0x102e34}});
    plan.compose("class-runtime", "encounters", {{EncounterRate::romOffset, EncounterRate::romOffset + EncounterRate::codeSize}});
    ExportPlan::Ranges shopReferences;
    for (const auto &value : shopConsumerOwners()) {
        const auto owner = value.toObject();
        const auto start = qsizetype(owner["start"].toInteger());
        for (const auto &entry : owner["cReferences"].toArray()) {
            const auto ref = entry.toObject();
            for (const auto *word : {"high", "low"})
                shopReferences.append({start + ref[word].toInt(), start + ref[word].toInt() + 4});
        }
    }
    // Accepted shop C has a different instruction schedule from the retail
    // assembly; its declared relocation words are the authoritative locations.
    plan.compose("class-runtime", "shops", shopReferences);
    const auto member = scenarioMemberOwner();
    const qsizetype memberStart = member["start"].toInteger();
    // The maintained constructor is compiled with the already relocated class
    // table, replacing its former pointer-load schedule in the same owner.
    plan.compose("scenarios", "class-runtime", {{memberStart, memberStart + member["size"].toInt()}});
    // Capacity selects the matching independent-member constructor variant,
    // rather than bypassing it with the ordinary native constructor.
    plan.compose("army-capacity", "scenarios", {{memberStart, memberStart + member["size"].toInt()}});
    ExportPlan::Ranges rewardCapacity;
    for (auto range : exportOwners("army-capacity"))
        if ((range.begin >= CreatureRewards::ownerOffset && range.end <= CreatureRewards::ownerOffset + CreatureRewards::ownerSize) ||
            (range.begin >= EncounterRate::romOffset && range.end <= EncounterRate::romOffset + EncounterRate::codeSize))
            rewardCapacity.append(range);
    plan.compose("army-capacity", "class-runtime", rewardCapacity);
    ExportPlan::Ranges iconSites, armySites, animationSites;
    const auto addSites = [](ExportPlan::Ranges &ranges, const auto &sites) {
        for (const auto &site : sites) {
            ranges.append({site.first, site.first+4});
            ranges.append({site.second, site.second+4});
        }
    };
    for (const auto &spec : IconArtwork::specs()) addSites(iconSites, spec.sites);
    for (const auto &spec : ArmyArtwork::specs()) addSites(armySites, spec.sites);
    addSites(animationSites, AnimationRoutes::sites());
    // Resource exporters change only qualified resource-key operands in these
    // earlier owners. Their existing loaders validate each instruction/target.
    for (const auto &earlier : {"class-runtime", "encounters", "custom-rewards", "shops", "scenarios"}) {
        plan.compose("item-icons", earlier, iconSites);
        plan.compose("army-artwork", earlier, armySites);
        plan.compose("animation-routes", earlier, animationSites);
    }
    // Existing code composes relocated bindings before installing the later
    // complete function or caller. Preserve that direction, never merely order.
    for (const auto &earlier : {"descriptions", "shops", "game-rules", "class-runtime"})
        composeCatalog(plan, "mission-tables", earlier);
    for (const auto &earlier : {"class-runtime", "shops", "mission-tables", "game-rules"})
        composeCatalog(plan, "army-capacity", earlier);
    for (const auto &earlier : {"class-runtime", "army-capacity", "animation-overrides", "high-attack"})
        composeCatalog(plan, "attack-references", earlier);
    for (const auto &earlier : {"class-runtime", "army-capacity", "mission-tables", "game-rules"})
        composeCatalog(plan, "movement", earlier);
    for (const auto &earlier : {"game-rules", "mission-tables", "army-capacity"})
        composeCatalog(plan, "mission-runtime", earlier);
    composeCatalog(plan, "class-runtime", "descriptions");
    composeCatalog(plan, "class-runtime", "shops");
    composeCatalog(plan, "high-attack", "class-runtime");
    composeCatalog(plan, "battlefield-loaders", "mission-tables");
}
}
