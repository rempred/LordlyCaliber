#include "engine/mission_runtime.h"
#include "engine/mission_tables.h"
#include "engine/class_data.h"
#include "engine/promotion.h"
namespace lordly {
QByteArray canonicalMissionImage(const Rom &rom) {
    auto image = rom.bytes();
    constexpr int settlement = 0x1390f0, settlementSize = 0xb20;
    image.replace(settlement, settlementSize,
                  MissionTables::canonicalCode(rom, settlement, settlementSize));
    const auto prior = ClassPlacement::read(rom);
    if (!prior || !prior->residentBytes) return image;
    const auto resident = image.mid(prior->residentAddress - ClassPlacement::residentDelta,
                                    prior->residentBytes);
    for (int at = 0; (at = resident.indexOf("LCMISN01", at)) >= 0; ++at) {
        if (at + 32 > resident.size()) continue;
        const auto metadata = resident.mid(at, 32);
        const auto size = u32(metadata, 12), address = u32(metadata, 20), entry = u32(metadata, 8);
        const qint64 begin = qint64(address) - prior->residentAddress;
        if (begin < 0 || begin + size != at || u32(metadata, 24) != 1 ||
            entry < address || quint64(entry) >= quint64(address) + size ||
            crc32(metadata.first(28)) != u32(metadata, 28) ||
            crc32(resident.mid(begin, size)) != u32(metadata, 16)) continue;
        const quint32 call = 0x0c000000U | ((entry >> 2) & 0x3ffffffU);
        int site = -1;
        for (int off = 0x1390f0; off < 0x139c10; off += 4) if (u32(image, off) == call) {
            if (site >= 0) throw Error("Multiple authored mission victory calls");
            site = off;
        }
        if (site >= 0) put32(image, site, 0x0c079534U); // native 0x801e54d0
    }
    return image;
}
}
