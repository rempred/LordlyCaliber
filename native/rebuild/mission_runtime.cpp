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

void installMissionEntryTitles(QByteArray &image) {
    // Director 1078 loads title resource 32 + battlefield key. Its original
    // directory ends at key 61, before the authored battlefield range.
    constexpr int resourceBase = 0x594280, originalKey = 0x01890c90;
    constexpr int keyHigh = 0x289404, keyLow = 0x289408;
    constexpr int firstTitle = 32, retailChildren = 94;
    constexpr int expandedChildren = firstTitle + MissionTables::keyCount;
    const int original = resourceBase + originalKey;
    const auto hi = u32(image, keyHigh), lo = u32(image, keyLow);
    if ((hi & 0xffff0000U) != 0x3c050000U || (lo & 0xffff0000U) != 0x34a50000U ||
        u32(image, original) != retailChildren * 4)
        throw Error("Unrecognized mission-entry title resource loader");
    const int current = int((hi & 65535U) << 16 | (lo & 65535U));

    // Use the native CI8 format with a transparent palette and empty artwork.
    const QByteArray card = QByteArray::fromHex("3634020100080016") + QByteArray(8 * 22 + 512, 0);
    const auto compressed = encodeLzss(card);
    QByteArray blank(8, 0);
    put32(blank, 0, quint32(compressed.size()) + 4);
    put32(blank, 4, quint32(card.size()));
    blank += compressed;
    while (blank.size() % 16) blank.append(char(-1));

    auto expandedDirectory = [&](int blankAt) {
        auto directory = image.mid(original, 4 + retailChildren * 4);
        directory.resize(4 + expandedChildren * 4);
        put32(directory, 0, expandedChildren * 4);
        for (int child = retailChildren; child < expandedChildren; ++child)
            put32(directory, 4 + child * 4, blankAt - resourceBase);
        return directory;
    };
    if (current != originalKey) {
        const qint64 directoryAt = qint64(resourceBase) + current;
        const qint64 blankAt = directoryAt - blank.size();
        if (blankAt < 0x2800000 || directoryAt % 16 ||
            directoryAt + 4 + expandedChildren * 4 > image.size() ||
            image.mid(blankAt, blank.size()) != blank ||
            image.mid(directoryAt, 4 + expandedChildren * 4) != expandedDirectory(int(blankAt)))
            throw Error("Unrecognized expanded mission-entry title directory");
        return;
    }

    const auto blankAt = (image.size() + 15) & ~qsizetype(15);
    const auto directoryAt = blankAt + blank.size();
    if (blankAt < 0x2800000 || directoryAt + 4 + expandedChildren * 4 > 0x4000000)
        throw Error("Mission-entry titles exceed cartridge space");
    const auto directory = expandedDirectory(int(blankAt));
    image += QByteArray(blankAt - image.size(), char(-1));
    image += blank;
    image += directory;
    const auto key = quint32(directoryAt - resourceBase);
    put32(image, keyHigh, 0x3c050000U | (key >> 16));
    put32(image, keyLow, 0x34a50000U | (key & 65535U));
}
}
MissionRuntimeBuild buildMissionRuntime(const Project &project, const Compiler &compiler,
                                       const QString &directory, quint32 address, quint32 capacity) {
    QByteArray rules(MissionTables::keyCount, 0);
    QByteArray briefingObjectives(MissionTables::keyCount, 0);
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
        if (objective > 0 && objective <= 255) put8(briefingObjectives, key, objective);
    }
    initMissionVictoryResources();
    QByteArray config("#define LC_OBJECTIVES {");
    for (int key = 0; key < rules.size(); ++key) {
        if (key) config += ',';
        config += QByteArray::number(u8(rules, key));
    }
    config += "}\n#define LC_BRIEFING_OBJECTIVES {";
    for (int key = 0; key < briefingObjectives.size(); ++key) {
        if (key) config += ',';
        config += QByteArray::number(u8(briefingObjectives, key));
    }
    config += "}\n";
    QFile source(":/game/scenarios/mission-victory.c");
    if (!source.open(QIODevice::ReadOnly)) throw Error("Missing authored mission victory C source");
    const auto code = compiler.compileModule(source.readAll(), directory, "mission_victory", address,
        {{"mission-victory-config.h", config}}, {{"lc_native_mission_victory", nativePredicate},
        {"lc_native_mission_briefing_site", nativeMissionBriefingSite}}, true);
    MissionRuntimeBuild result{code.bytes, code.entries, !keys.isEmpty()};
    while (result.bytes.size() % 16) result.bytes.append(char(0));
    QByteArray metadata(48, 0);
    metadata.replace(0, 8, "LCMISN02");
    put32(metadata, 8, result.symbols.value("lc_authored_mission_victory"));
    put32(metadata, 12, quint32(result.bytes.size()));
    put32(metadata, 16, crc32(result.bytes));
    put32(metadata, 20, address);
    // Older victory-only modules stored 1 in this word.
    put32(metadata, 24, result.symbols.value("lc_authored_mission_briefing_site"));
    put32(metadata, 28, result.symbols.value("lc_authored_mission_exit"));
    put32(metadata, 44, crc32(metadata.first(44)));
    result.bytes += metadata;
    if (!result.symbols.value("lc_authored_mission_victory") ||
        !result.symbols.value("lc_authored_mission_briefing_site") ||
        !result.symbols.value("lc_authored_mission_exit") || quint32(result.bytes.size()) > capacity)
        throw Error("The authored mission runtime exceeds its resident allocation");
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
    for (int at : missionBriefingCalls)
        if (u32(restored, at) == u32(project.rom().bytes(), at))
            put32(restored, at, u32(priorCanonical, at));
    if (restored.mid(missionExitDispatch, 16) == project.rom().bytes().mid(missionExitDispatch, 16))
        restored.replace(missionExitDispatch, 16, priorCanonical.mid(missionExitDispatch, 16));
    if (build.bytes.isEmpty()) return restored;
    if (restored.mid(missionExitDispatch, 16) != missionExitDispatchCode())
        throw Error("Unrecognized native mission mode dispatch");
    image = restored;
    image.replace(missionExitDispatch, 16, missionExitDispatchCode(build.symbols.value("lc_authored_mission_exit")));
    if (!build.hasBattlefields) return image;
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
    for (int at : missionBriefingCalls) {
        if (u32(restored, at) != callWord(nativeMissionBriefingSite) || u32(restored, at + 4) != 0)
            throw Error("Unrecognized native mission briefing site lookup");
        put32(image, at, callWord(build.symbols.value("lc_authored_mission_briefing_site")));
    }
    installMissionEntryTitles(image);
    return image;
}
}
