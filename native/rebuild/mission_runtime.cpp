#include "rebuild/mission_runtime.h"
#include "engine/class_data.h"
#include "engine/capacity_placement.h"
#include "engine/goth_runtime.h"
#include "engine/mission_runtime.h"
#include "engine/mission_tables.h"
#include "engine/promotion.h"
#include <QFile>
#include <QJsonArray>
#include <QSet>
static void initMissionVictoryResources() { Q_INIT_RESOURCE(mission_victory_c); }
namespace lordly {
namespace {
constexpr int ownerStart = 0x1390f0;
constexpr quint32 nativePredicate = 0x801e54d0;
constexpr quint32 callWord(quint32 address) { return 0x0c000000U | ((address >> 2) & 0x3ffffffU); }
}
MissionRuntimeBuild buildMissionRuntime(const Project &project, const Compiler &compiler,
                                       const QString &directory, quint32 address, quint32 capacity) {
    QByteArray rules(MissionTables::keyCount, 0);
    QSet<int> keys;
    for (const auto &value : project.scenarioBattlefields()) {
        const auto row = value.toObject();
        const int key = row["key"].toInt();
        const int objective = row["snapshot"].toObject()["units"].toObject()["objective"].toInt();
        const int donor = row["source"].toInt();
        const bool exact = row["mode"].toString() == "exact";
        if (exact && QSet<int>{10, 22, 35, 62}.contains(donor))
            throw Error("An exact battlefield copy needs a retail source with a supported victory rule");
        if (!MissionTables::authoringKey(key) || keys.contains(key) || donor < 1 || donor > 64 ||
            (!exact && (objective < 1 || objective > 20)))
            throw Error("A battlefield without squads needs a valid enemy stronghold objective");
        keys.insert(key);
        put8(rules, key, exact ? 128 | donor : objective);
    }
    if (keys.isEmpty()) return {};
    initMissionVictoryResources();
    QByteArray config("#define LC_OBJECTIVES {");
    for (int key = 0; key < rules.size(); ++key) {
        if (key) config += ',';
        config += QByteArray::number(u8(rules, key));
    }
    config += "}\n";
    QFile source(":/game/scenarios/mission-victory.c");
    if (!source.open(QIODevice::ReadOnly)) throw Error("Missing authored mission victory C source");
    const auto code = compiler.compileModule(source.readAll(), directory, "mission_victory", address,
        {{"mission-victory-config.h", config}}, {{"lc_native_mission_victory", nativePredicate}}, true);
    MissionRuntimeBuild result{code.bytes, code.entries};
    while (result.bytes.size() % 16) result.bytes.append(char(0));
    QByteArray metadata(32, 0);
    metadata.replace(0, 8, "LCMISN01");
    put32(metadata, 8, result.symbols.value("lc_authored_mission_victory"));
    put32(metadata, 12, quint32(result.bytes.size()));
    put32(metadata, 16, crc32(result.bytes));
    put32(metadata, 20, address);
    put32(metadata, 24, 1);
    put32(metadata, 28, crc32(metadata.first(28)));
    result.bytes += metadata;
    if (!result.symbols.value("lc_authored_mission_victory") || quint32(result.bytes.size()) > capacity)
        throw Error("Authored mission victory exceeds its resident allocation");
    return result;
}
QByteArray linkMissionRuntime(const Rom &rom, const Project &project, const MissionRuntimeBuild &build,
                             const Compiler &compiler, const QString &directory) {
    // Qualify the complete accepted settlement C owner, then redirect its one
    // predicate call. Its rewards, captured-site processing and report stay intact.
    auto image = rom.bytes();
    auto restored = image;
    const auto priorCanonical = canonicalMissionImage(project.rom());
    const auto priorTableCanonical =
        MissionTables::canonicalCode(project.rom(), ownerStart, 0x139c10 - ownerStart);
    for (int at = ownerStart; at < 0x139c10; at += 4)
        if (u32(priorCanonical, at) != u32(priorTableCanonical, at - ownerStart) &&
            u32(restored, at) == u32(project.rom().bytes(), at))
            put32(restored, at, u32(priorCanonical, at));
    if (build.bytes.isEmpty()) return restored;
    auto normalized = Rom::fromBytes(restored);
    if (const auto capacity = CapacityPlacement::read(normalized))
        normalized = capacity->canonical(normalized);
    const auto settlementCatalog = GothRuntime::definition();
    auto accepted = compiler.compileQualifiedOwner(settlementCatalog["retail"].toObject(),
                                                   directory, "mission_settlement_original").code;
    if (const auto goth = GothRuntime::read(normalized)) {
        accepted = compiler.compileQualifiedOwner(settlementCatalog[goth->reserve ? "reserve" : "plain"].toObject(),
                                                  directory, "mission_settlement_configured").code;
        accepted += normalized.bytes().mid(GothRuntime::headerOffset, 16);
    }
    int call = -1;
    for (int at = 0; at + 4 <= accepted.size(); at += 4) if (u32(accepted, at) == callWord(nativePredicate)) {
        if (call >= 0) throw Error("The mission settlement predicate is not unique");
        call = ownerStart + at;
    }
    if (call < 0) throw Error("Missing native mission settlement predicate");
    auto actualOwner = MissionTables::canonicalCode(normalized, ownerStart, accepted.size());
    if (const auto classes = ClassPlacement::read(normalized))
        actualOwner = classes->canonicalReferences(actualOwner, ownerStart);
    if (actualOwner != accepted)
        throw Error("Mission settlement differs from its qualified accepted C owner");
    put32(image, call, callWord(build.symbols.value("lc_authored_mission_victory")));
    return image;
}
}
