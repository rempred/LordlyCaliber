#include "engine/mission_battlefield.h"
#include "engine/scenario_map.h"
#include "engine/squad_members.h"
#include <QJsonArray>
#include <QSet>

namespace lordly {
namespace {
QString hex(const QByteArray &bytes) { return QString::fromLatin1(bytes.toHex()); }
QByteArray bytes(const QJsonValue &value, int length = -1) {
    const auto text = value.toString().toLatin1();
    const auto data = QByteArray::fromHex(text);
    if (!value.isString() || data.toHex() != text.toLower() ||
        (length >= 0 && data.size() != length))
        throw Error("Invalid captured battlefield bytes");
    return data;
}
void requireMode(const QString &mode) {
    if (mode != "exact" && mode != "mapOnly" && mode != "blank")
        throw Error("Choose copy map and squads, copy map without squads, or blank map");
}
void requireTarget(int target) {
    if (!MissionTables::authoringKey(target))
        throw Error("A new battlefield needs an unused scenario slot");
}
QByteArray compiledLayout(const QJsonObject &value, QByteArray &library) {
    auto layout = Eset::decode(bytes(value["layout"]));
    const auto squads = value["squads"].toArray();
    if (!value["squads"].isArray() || squads.size() != layout.deployments.size())
        throw Error("Captured squads and deployment rows disagree");
    QMap<QByteArray, int> records;
    for (int id = 1; id <= library.size() / 35; ++id) {
        const auto record = readSquadRecord(library, id);
        if (!records.contains(record)) records[record] = id;
    }
    for (int i = 0; i < squads.size(); ++i) {
        const auto record = bytes(squads[i]);
        squadMembers(record);
        int id = records.value(record);
        if (!id) {
            id = library.size() / 35 + 1;
            if (id - 1 + record.size() / 35 > 65535)
                throw Error("Captured squad library exceeds native template IDs");
            records[record] = id;
            library += record;
        }
        put16(layout.deployments[i], 1, quint16(id));
    }
    return layout.encode();
}
} // namespace

QJsonObject MissionBattlefield::capture(const Rom &rom, const Scenarios &scenarios,
                                        const ScenarioWorld &world, int source,
                                        const QString &mode) {
    requireMode(mode);
    const auto &original = scenarios.scenario(source);
    const auto descriptor = ScenarioResources::row(rom, source);
    if (descriptor.size() != 9 || u8(descriptor, 3) < 5 || u8(descriptor, 3) > 41)
        throw Error("Choose an ordinary battlefield as the starting map");
    const auto map = ScenarioMap::load(rom, source);
    const int siteCount = world.siteCount(source);
    if (!siteCount || map.points.size() != siteCount)
        throw Error("This map uses a special site initializer; choose another starting map");
    auto scenario = original;
    if (mode != "exact") {
        scenario.squads.clear();
        scenario.layout.deployments.clear();
        scenario.layout.conditions.clear();
        scenario.layout.conditionsPresent = false;
        scenario.layout.seed = QByteArray(2, 0);
        QByteArray node(18, 0);
        node[0] = 4;
        scenario.layout.nodes = {node};
        scenario.layout = Eset::decode(scenario.layout.encode());
    }
    auto sites = world.scenario(source).sites;
    auto treasures = world.scenario(source).treasures;
    int objective = -1;
    for (int i = 0; i < siteCount; ++i)
        if (u8(world.town(u16(sites, 1 + i * 4)), 24) == 255) {
            objective = i;
            break;
        }
    if (mode != "exact" && (objective < 0 || objective >= 20))
        throw Error("Choose a starting map with an enemy objective among its first 20 sites");
    if (mode == "mapOnly")
        put16(sites, 3 + objective * 4, 4);
    QJsonArray pointRows;
    if (mode == "blank") {
        if (siteCount < 2 || objective < 1)
            throw Error("A blank map needs a starting map with a home site and enemy objective");
        pointRows = {0, objective};
        QByteArray blankSites(1, char(2));
        blankSites += sites.mid(1, 4);
        blankSites += sites.mid(1 + objective * 4, 4);
        put16(blankSites, 3, 0x2012);
        put16(blankSites, 7, 4);
        sites = blankSites;
        treasures = QByteArray(1, 0);
    } else {
        const int count = ScenarioWorld::treasureCount(treasures);
        const auto available = world.availableTreasureFlags();
        if (available.size() < count)
            throw Error("There are not enough independent collection flags to copy these treasures");
        for (int i = 0; i < count; ++i)
            put8(treasures, 1 + i * 6, quint8(available[i]));
        for (int i = 0; i < siteCount; ++i) pointRows.append(i);
    }
    return {{"version", 1}, {"source", source}, {"mode", mode},
            {"selector", hex(descriptor)},
            {"worldSelector", hex(WorldResources::row(rom, source))},
            {"boundary", hex(MissionTables::row(rom, MissionTables::Boundary, source))},
            {"camera", hex(MissionTables::row(rom, MissionTables::Camera, source))},
            {"scenario", scenario.json()},
            {"world", QJsonObject{{"sites", hex(sites)}, {"treasures", hex(treasures)}}},
            {"objective", mode == "blank" ? 2 : objective + 1},
            {"pointRows", pointRows}};
}

QByteArray MissionBattlefield::prepare(const Rom &rom, int target,
                                      const QJsonObject &snapshot) {
    requireTarget(target);
    requireMode(snapshot["mode"].toString());
    const int source = snapshot["source"].toInt(-1);
    if (snapshot["version"] != 1 || source < 1 || source >= 65 || source == 54 ||
        source == target || !snapshot["scenario"].isObject() || !snapshot["world"].isObject())
        throw Error("Invalid captured battlefield identity");
    auto selector = bytes(snapshot["selector"], 9);
    auto worldSelector = bytes(snapshot["worldSelector"], 12);
    const auto boundary = bytes(snapshot["boundary"], 4);
    const auto camera = snapshot.contains("camera") ? bytes(snapshot["camera"], 12)
        : MissionTables::row(rom, MissionTables::Camera, source);
    auto image = rom.bytes();
    MissionTables::ensure(image, target);
    const auto expanded = Rom::fromBytes(image);
    auto resources = ScenarioResources::load(expanded);
    auto library = resources.enemyData();
    const auto layout = compiledLayout(snapshot["scenario"].toObject(), library);
    image = resources.rebuild(expanded, library, {{target, layout}});
    const auto current = Rom::fromBytes(image);
    const auto worlds = WorldResources::load(current);
    const auto world = snapshot["world"].toObject();
    const auto sites = bytes(world["sites"]), treasures = bytes(world["treasures"]);
    const int siteCount = ScenarioWorld::declaredSiteCount(sites);
    const int treasureCount = ScenarioWorld::treasureCount(treasures);
    if (!siteCount || sites.size() < 1 + siteCount * 4 ||
        treasures.isEmpty() || treasureCount > ScenarioWorld::treasureCapacity)
        throw Error("Captured battlefield sites or treasures are incomplete");
    if (snapshot["mode"] != "exact") {
        const int objective = snapshot["objective"].toInt();
        if (objective < 1 || objective > 20 || objective > siteCount)
            throw Error("Captured battlefield needs an enemy objective among its first 20 sites");
    }
    for (int i = 0; i < siteCount; ++i)
        if (u16(sites, 1 + i * 4) < 1 || u16(sites, 1 + i * 4) > ScenarioWorld::townCount)
            throw Error("Captured battlefield references a missing town");
    image = worlds.rebuild(current, worlds.towns(current), {{target, treasures}}, {{target, sites}});
    // Resource selectors belong to the checked resource rebuild allocations.
    // Copy the remaining native per-key settings after those selectors exist.
    const auto compiled = Rom::fromBytes(image);
    const auto esetRow = ScenarioResources::row(compiled, target);
    const auto worldRow = WorldResources::row(compiled, target);
    put8(selector, 8, u8(esetRow, 8));
    put8(worldSelector, 5, u8(worldRow, 5));
    put8(worldSelector, 6, u8(worldRow, 6));
    MissionTables::replace(image, MissionTables::Scenario, target, selector);
    MissionTables::replace(image, MissionTables::World, target, worldSelector);
    MissionTables::replace(image, MissionTables::Boundary, target, boundary);
    if (snapshot.contains("camera") || MissionTables::newKey(target))
        MissionTables::replace(image, MissionTables::Camera, target, camera);
    return image;
}
} // namespace lordly
