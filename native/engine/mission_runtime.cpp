#include "engine/mission_runtime.h"
#include "engine/mission_tables.h"
#include "engine/class_data.h"
#include "engine/promotion.h"
namespace lordly {
namespace {
struct MissionModule {
    quint32 victory, briefing, exit;
};
std::optional<MissionModule> missionModule(const QByteArray &resident, quint32 base, int at) {
    const bool withExit = resident.mid(at, 8) == "LCMISN02";
    const int metadataSize = withExit ? 48 : 32;
    if ((!withExit && resident.mid(at, 8) != "LCMISN01") || at + metadataSize > resident.size()) return {};
    const auto metadata = resident.mid(at, metadataSize);
    const auto size = u32(metadata, 12), address = u32(metadata, 20);
    const MissionModule entries{u32(metadata, 8), u32(metadata, 24), withExit ? u32(metadata, 28) : 0};
    const qint64 begin = qint64(address) - base;
    const auto entryInCode = [&](quint32 target) {
        return !(target & 3) && target >= address && quint64(target) < quint64(address) + size;
    };
    if (begin < 0 || begin + size != at || !entryInCode(entries.victory) ||
        (entries.briefing != 1 && !entryInCode(entries.briefing)) ||
        (withExit && !entryInCode(entries.exit)) ||
        crc32(metadata.first(metadataSize - 4)) != u32(metadata, metadataSize - 4) ||
        crc32(resident.mid(begin, size)) != u32(metadata, 16)) return {};
    return entries;
}
}
QByteArray missionExitDispatchCode(quint32 entry) {
    if (!entry) return QByteArray::fromHex("3c02800f9442810e104000253402fffe");
    QByteArray code(16, 0);
    put32(code, 0, 0x0c000000U | ((entry >> 2) & 0x3ffffffU));
    put32(code, 8, 0x14400025U); // Nonzero override joins the original mode store.
    return code;
}

QByteArray canonicalMissionExitCode(const QByteArray &image) {
    const auto code = image.mid(missionExitDispatch, 16);
    if (code.size() != 16 || u32(code, 0) >> 26 != 3) return code;
    const quint32 entry = 0x80000000U | ((u32(code, 0) & 0x3ffffffU) << 2);
    if (code != missionExitDispatchCode(entry)) return code;

    // Save-owner validation calls this before ClassPlacement::read completes.
    // Authenticate the bounded resident payload directly to avoid recursion.
    const auto header = image.mid(ClassPlacement::descriptorRomOffset, ClassPlacement::descriptorSize);
    if (header.size() != ClassPlacement::descriptorSize || !header.startsWith("LC05CLAS") ||
        u32(header, 8) < 4 || u32(header, 8) > 6 ||
        crc32(header.first(124)) != u32(header, 124)) return code;
    const auto address = u32(header, 104), size = u32(header, 108);
    if (address < ClassPlacement::residentBegin || ((address | size) & 15) ||
        quint64(address) + size > ClassPlacement::residentEnd ||
        entry < address || quint64(entry) >= quint64(address) + size) return code;
    const auto resident = image.mid(address - ClassPlacement::residentDelta, size);
    if (resident.size() != qsizetype(size) || crc32(resident) != u32(header, 112)) return code;
    for (int at = 0; (at = resident.indexOf("LCMISN02", at)) >= 0; ++at) {
        const auto module = missionModule(resident, address, at);
        if (module && module->exit == entry) return missionExitDispatchCode();
    }
    return code;
}

QByteArray canonicalMissionImage(const Rom &rom) {
    auto image = rom.bytes();
    constexpr int settlement = 0x1390f0, settlementSize = 0xb20;
    image.replace(settlement, settlementSize,
                  MissionTables::canonicalCode(rom, settlement, settlementSize));
    const auto prior = ClassPlacement::read(rom);
    if (!prior || !prior->residentBytes) return image;
    const auto resident = image.mid(prior->residentAddress - ClassPlacement::residentDelta,
                                    prior->residentBytes);
    for (int at = 0; (at = resident.indexOf("LCMISN0", at)) >= 0; ++at) {
        const auto module = missionModule(resident, prior->residentAddress, at);
        if (!module) continue;
        const auto [entry, briefingEntry, exitEntry] = *module;
        const quint32 call = 0x0c000000U | ((entry >> 2) & 0x3ffffffU);
        int site = -1;
        for (int off = 0x1390f0; off < 0x139c10; off += 4) if (u32(image, off) == call) {
            if (site >= 0) throw Error("Multiple authored mission victory calls");
            site = off;
        }
        if (site >= 0) put32(image, site, 0x0c079534U); // native 0x801e54d0
        if (briefingEntry != 1) {
            const quint32 briefingCall = 0x0c000000U | ((briefingEntry >> 2) & 0x3ffffffU);
            const quint32 nativeCall = 0x0c000000U | ((nativeMissionBriefingSite >> 2) & 0x3ffffffU);
            for (int off : missionBriefingCalls)
                if (u32(image, off) == briefingCall) put32(image, off, nativeCall);
        }
        if (exitEntry && image.mid(missionExitDispatch, 16) == missionExitDispatchCode(exitEntry))
            image.replace(missionExitDispatch, 16, missionExitDispatchCode());
    }
    return image;
}
}
