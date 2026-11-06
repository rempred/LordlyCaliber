#include "engine/mission_tables.h"
#include "engine/promotion.h"
#include "engine/class_data.h"
#include "engine/shop_runtime.h"
#include <array>

namespace lordly {
namespace {
constexpr int headerSize = 64, rows = MissionTables::keyCount - MissionTables::firstKey;
constexpr int rowSize = 37, allocationSize = rows * rowSize;
constexpr int scenarioRuntimeSize = (MissionTables::keyCount * 9 + 3) & ~3;
constexpr int boundaryRuntimeOffset = scenarioRuntimeSize + MissionTables::keyCount * 12;
constexpr int cameraRuntimeOffset = boundaryRuntimeOffset + MissionTables::keyCount * 4;
constexpr int runtimeSize = (cameraRuntimeOffset + MissionTables::keyCount * 12 + 15) & ~15;
struct TableLayout { int original, width, offset, runtime, nativeCount; };
constexpr TableLayout layouts[] = {
    {0x654a0, 9, 0, 0, 69}, {0x13c550, 12, 9, scenarioRuntimeSize, 67},
    {0x13c874, 4, 21, boundaryRuntimeOffset, 67},
    {0x13c980, 12, 25, cameraRuntimeOffset, 67}
};
struct Placement { int offset = 0; quint32 runtime = 0; };
Placement placement(const QByteArray &bytes) {
    const auto h = bytes.mid(MissionTables::descriptorOffset, headerSize);
    if (h == QByteArray(headerSize, char(-1))) return {};
    if (h.size() != headerSize || !h.startsWith("LCMSTBL1") || u32(h, 8) != 1 ||
        u32(h, 60) != crc32(h.first(60)))
        throw Error("Unrecognized extended mission table descriptor");
    const auto offset = u32(h, 12), runtime = u32(h, 16);
    if (offset < 0x2800000 || offset % 16 || offset > 0x4000000 - allocationSize ||
        quint64(offset) + allocationSize > quint64(bytes.size()) ||
        (runtime && (runtime % 16 ||
            !((runtime >= 0x80187f00 && runtime <= 0x8018aa70 - runtimeSize) ||
              (runtime >= 0x80243db0 && runtime <= 0x80400000 - runtimeSize)))) ||
        u32(h, 20) != crc32(bytes.mid(offset, allocationSize)))
        throw Error("Extended mission tables differ from their recorded allocation");
    return {int(offset), runtime};
}
void record(QByteArray &bytes, Placement p) {
    QByteArray h(headerSize, 0);
    h.replace(0, 8, "LCMSTBL1");
    put32(h, 8, 1);
    put32(h, 12, p.offset);
    put32(h, 16, p.runtime);
    put32(h, 20, crc32(bytes.mid(p.offset, allocationSize)));
    put32(h, 60, crc32(h.first(60)));
    bytes.replace(MissionTables::descriptorOffset, headerSize, h);
}
int at(MissionTables::Table table, int key, Placement p) {
    const auto &t = layouts[table];
    return MissionTables::newKey(key) ? p.offset + (key - MissionTables::firstKey) * rowSize + t.offset
                                       : t.original + key * t.width;
}
QByteArray readRow(const QByteArray &bytes, MissionTables::Table table, int key, Placement p) {
    const auto &t = layouts[table];
    if (key < 0 || key >= MissionTables::keyCount)
        throw Error("Mission key exceeds its native table");
    if (MissionTables::newKey(key) && !p.offset)
        return table == MissionTables::Scenario ? QByteArray::fromHex("000000000000ff0000")
                                                : QByteArray(t.width, 0);
    if (!MissionTables::newKey(key) && key >= t.nativeCount)
        return QByteArray(t.width, 0);
    return bytes.mid(at(table, key, p), t.width);
}
struct Reference {
    quint32 highOffset, lowOffset, originalHigh, originalLow;
    MissionTables::Table table;
    int field;
    bool unsignedLoad = false;
};
using T = MissionTables;
constexpr Reference references[] = {
    {0x0062b4,0x0062bc,0x3c028019,0x9042f5a3,T::Scenario,3},
    {0x0ebe08,0x0ebe10,0x3c018019,0x9023f5a3,T::Scenario,3},
    {0x0ebf34,0x0ebf3c,0x3c118019,0x8231f5a4,T::Scenario,4},
    {0x0ebf9c,0x0ebfa4,0x3c118019,0x8231f5a5,T::Scenario,5},
    {0x0ec120,0x0ec128,0x3c118019,0x8231f5a7,T::Scenario,7},
    {0x0ec198,0x0ec1a0,0x3c118019,0x8231f5a8,T::Scenario,8,true},
    {0x0edcec,0x0edcf4,0x3c068019,0x80c6f5a7,T::Scenario,7},
    {0x0edd98,0x0edda0,0x3c028019,0x8042f5a3,T::Scenario,3},
    {0x2b50d8,0x2b50e0,0x3c058019,0x80a5f5a3,T::Scenario,3},
    {0x101e4c,0x101e54,0x3c05801e,0x90a57e17,T::World,7},
    {0x1278f8,0x127900,0x3c07801e,0x90e77e15,T::World,5},
    {0x130dc4,0x130dcc,0x3c02801e,0x90427e13,T::World,3},
    {0x130e28,0x130e30,0x3c01801e,0x90227e14,T::World,4},
    {0x1312ac,0x1312b4,0x3c02801e,0x90427e13,T::World,3},
    {0x131310,0x131318,0x3c03801e,0x90637e14,T::World,4},
    {0x13c0a0,0x13c0a8,0x3c02801e,0x90427e18,T::World,8},
    {0x13c3ac,0x13c3b4,0x3c10801e,0x92107e18,T::World,8},
    {0x196158,0x196160,0x3c07801e,0x90e77e16,T::World,6},
    {0x103244,0x10324c,0x3c02801f,0x90428134,T::Boundary,0},
    {0x103284,0x10328c,0x3c02801f,0x90428135,T::Boundary,1},
    {0x1032b0,0x1032b8,0x3c02801f,0x90428136,T::Boundary,2},
    {0x1032dc,0x1032e4,0x3c02801f,0x90428137,T::Boundary,3},
    {0x115b64,0x115b6c,0x3c03801f,0x90638134,T::Boundary,0},
    {0x115b94,0x115b98,0x3c07801f,0x24e78134,T::Boundary,0},
    {0x115b9c,0x115ba4,0x3c0a801f,0x914a8137,T::Boundary,3},
    {0x102e64,0x102e6c,0x3c01801f,0x90228240,T::Camera,0},
    {0x102e84,0x102e8c,0x3c01801f,0x90228241,T::Camera,1},
    {0x102ea4,0x102eac,0x3c01801f,0x90228242,T::Camera,2},
    {0x102ec4,0x102ecc,0x3c01801f,0x90228243,T::Camera,3},
    {0x1154d0,0x1154d8,0x3c02801f,0x90428240,T::Camera,0}
};
quint32 addressOf(const Reference &ref, quint32 runtime) {
    return runtime + layouts[ref.table].runtime + ref.field;
}
quint32 highWord(const Reference &ref, quint32 runtime) {
    return (ref.originalHigh & 0xffff0000) | ((addressOf(ref, runtime) + 0x8000) >> 16);
}
quint32 lowWord(const Reference &ref, quint32 runtime) {
    return (ref.originalLow & 0xffff0000) | (ref.unsignedLoad ? 0x10000000 : 0) |
           (addressOf(ref, runtime) & 65535);
}
// The accepted Goth variants preserve the payout load but may schedule it
// at another instruction address within the authenticated settlement owner.
Reference payoutReference(const QByteArray &bytes, quint32 runtime) {
    Reference found{};
    for (quint32 at = 0x1390f8; at < 0x139c10; at += 4) {
        const auto lo = u32(bytes, at);
        const auto reg = (lo >> 21) & 31U;
        if ((lo >> 26) != 37) continue;
        quint32 highAt = at - 8;
        // Configured Goth C schedules the LHU in a branch delay slot.
        // Its LUI/ADDU pair is twelve bytes before the load, not eight.
        if ((u32(bytes, highAt) & 0xffff0000) != (0x3c000000U | reg << 16)) highAt -= 4;
        const auto hi = u32(bytes, highAt), add = u32(bytes, highAt + 4);
        if ((hi & 0xffff0000) != (0x3c000000U | reg << 16) ||
            (add & 0xffe0ffff) != (reg << 21 | reg << 11 | 0x21)) continue;
        const Reference candidate{highAt, at, 0x3c00801eU | reg << 16,
                                  (lo & 0xffff0000U) | 0x7e1a, T::World, 10};
        if ((hi != candidate.originalHigh || lo != candidate.originalLow) &&
            (!runtime || hi != highWord(candidate, runtime) || lo != lowWord(candidate, runtime)))
            continue;
        if (found.lowOffset) throw Error("Multiple mission settlement payout table loads");
        found = candidate;
    }
    if (!found.lowOffset) throw Error("Missing mission settlement payout table load");
    return found;
}
Reference shopReference(const QByteArray &bytes, quint32 runtime) {
    constexpr Reference retail{0x19be90, 0x19be98, 0x3c07801e, 0x90e77e15, T::World, 5};
    const auto shop = bytes.mid(ShopPlacement::descriptorOffset, ShopPlacement::descriptorSize);
    if (!shop.startsWith("LC05SHOP") || u32(shop, 8) < 4) return retail;
    // Accepted stock C schedules the site selector at a different instruction.
    // Its complete owner is authenticated by ShopPlacement before export.
    Reference found{};
    for (quint32 at = ShopPlacement::stockOffset + 8; at < ShopPlacement::guardOffset; at += 4) {
        const Reference ref{at - 8, at, retail.originalHigh, retail.originalLow, T::World, 5};
        const auto hi = u32(bytes, ref.highOffset), lo = u32(bytes, ref.lowOffset);
        if (u32(bytes, at - 4) != 0x00e23821 ||
            ((hi != ref.originalHigh || lo != ref.originalLow) &&
             (!runtime || hi != highWord(ref, runtime) || lo != lowWord(ref, runtime)))) continue;
        if (found.lowOffset) throw Error("Multiple rebuilt shop site selector loads");
        found = ref;
    }
    if (!found.lowOffset) throw Error("Missing rebuilt shop site selector load");
    return found;
}
constexpr int modelSearchLimit = 0x0eddac;
constexpr quint32 originalSearch = 0x28820045;
constexpr quint32 expandedSearch = 0x28820000 | MissionTables::keyCount;
}
bool MissionTables::hasScenario(const Rom &rom, int key) {
    if (key >= 1 && key < retailKeyCount) return true;
    return newKey(key) && u8(row(rom, Scenario, key), 3) != 0;
}
QByteArray MissionTables::row(const Rom &rom, Table table, int key) {
    return readRow(rom.bytes(), table, key, newKey(key) ? placement(rom.bytes()) : Placement{});
}
void MissionTables::ensure(QByteArray &bytes, int key) {
    if (!newKey(key) || placement(bytes).offset) return;
    const auto offset = (bytes.size() + 15) & ~qsizetype(15);
    if (offset < 0x2800000 || offset + allocationSize > 0x4000000)
        throw Error("Extended mission tables exceed cartridge space");
    bytes += QByteArray(offset - bytes.size() + allocationSize, 0);
    for (int k = firstKey; k < keyCount; ++k)
        put8(bytes, at(Scenario, k, {int(offset), 0}) + 6, 255);
    record(bytes, {int(offset), 0});
}
void MissionTables::replace(QByteArray &bytes, Table table, int key, const QByteArray &value) {
    if (key < 0 || key >= keyCount || value.size() != layouts[table].width)
        throw Error("Invalid mission table row");
    ensure(bytes, key);
    const auto p = newKey(key) ? placement(bytes) : Placement{};
    bytes.replace(at(table, key, p), value.size(), value);
    if (p.offset) record(bytes, p);
}
void MissionTables::updateSelectors(QByteArray &bytes, Table table, const QByteArray &selectors) {
    const auto p = placement(bytes);
    const int count = p.offset ? keyCount : retailKeyCount;
    const int stride = table == Scenario ? 1 : 2;
    if (table > World || selectors.size() != count * stride)
        throw Error("Invalid extended mission selector directory");
    for (int key = 0; key < count; ++key) {
        if (key >= retailKeyCount && !newKey(key)) continue;
        const int where = at(table, key, p);
        if (table == Scenario) put8(bytes, where + 8, u8(selectors, key));
        else {
            put8(bytes, where + 6, u8(selectors, key * 2));
            put8(bytes, where + 5, u8(selectors, key * 2 + 1));
        }
    }
    if (p.offset) record(bytes, p);
}
quint32 MissionTables::worldAddress(quint32 runtime) {
    return runtime ? runtime + scenarioRuntimeSize : 0;
}
QByteArray MissionTables::runtimeData(const Rom &rom) {
    const auto &bytes = rom.bytes();
    const auto p = placement(bytes);
    if (!p.offset) return {};
    QByteArray result(runtimeSize, 0);
    for (int n = Scenario; n <= Camera; ++n) {
        const auto table = Table(n);
        const auto &t = layouts[n];
        for (int key = 0; key < keyCount; ++key) {
            // Keep the exact effective reads for hidden native keys, including
            // the adjacent bytes beyond the public world/camera directories.
            const auto value = key >= t.nativeCount && key < firstKey
                ? bytes.mid(t.original + key * t.width, t.width)
                : readRow(bytes, table, key, p);
            result.replace(t.runtime + key * t.width, t.width, value);
        }
    }
    return result;
}
QByteArray MissionTables::canonicalCode(const Rom &rom, int offset, int length) {
    auto out = rom.bytes().mid(offset, length);
    const auto runtime = placement(rom.bytes()).runtime;
    if (!runtime) return out;
    auto restore = [&](quint32 at, quint32 relocated, quint32 original) {
        if (at >= quint32(offset) && at + 4 <= quint32(offset + length) &&
            u32(out, at - offset) == relocated)
            put32(out, at - offset, original);
    };
    for (const auto &ref : references) {
        if (ref.highOffset) restore(ref.highOffset, highWord(ref, runtime), ref.originalHigh);
        restore(ref.lowOffset, lowWord(ref, runtime), ref.originalLow);
    }
    if (offset < ShopPlacement::stockOffset + ShopPlacement::stockSize &&
        offset + length > ShopPlacement::stockOffset) {
        const auto ref = shopReference(rom.bytes(), runtime);
        restore(ref.highOffset, highWord(ref, runtime), ref.originalHigh);
        restore(ref.lowOffset, lowWord(ref, runtime), ref.originalLow);
    }
    if (offset < 0x139c10 && offset + length > 0x1390f0) {
        const auto ref = payoutReference(rom.bytes(), runtime);
        restore(ref.highOffset, highWord(ref, runtime), ref.originalHigh);
        restore(ref.lowOffset, lowWord(ref, runtime), ref.originalLow);
    }
    restore(modelSearchLimit, expandedSearch, originalSearch);
    return out;
}
void MissionTables::linkRuntime(QByteArray &bytes, quint32 address) {
    auto p = placement(bytes);
    if (!p.offset) return;
    if (address < 0x80243db0 || address > 0x80400000 - runtimeSize || address % 16)
        throw Error("Extended mission tables exceed permanent storage");
    const auto rom = Rom::fromBytes(bytes);
    const auto data = runtimeData(rom);
    auto resident = ClassPlacement::read(rom);
    if (!resident || address < resident->layout.indexBase ||
        address + data.size() > resident->layout.end)
        throw Error("Extended mission tables need a declared permanent allocation");
    const auto payout = payoutReference(bytes, p.runtime);
    const auto shop = shopReference(bytes, p.runtime);
    auto link = [&](quint32 at, quint32 original, quint32 prior, quint32 next) {
        const auto current = u32(bytes, at);
        if (current != original && current != prior && current != next)
            throw Error(QString("Unsupported mission table consumer at z64 ROM 0x%1").arg(at, 0, 16));
        put32(bytes, at, next);
    };
    auto relocate = [&](const Reference &ref) {
        if (ref.highOffset)
            link(ref.highOffset, ref.originalHigh, p.runtime ? highWord(ref, p.runtime) : ref.originalHigh,
                 highWord(ref, address));
        link(ref.lowOffset, ref.originalLow, p.runtime ? lowWord(ref, p.runtime) : ref.originalLow,
             lowWord(ref, address));
    };
    for (const auto &ref : references) relocate(ref);
    relocate(payout);
    relocate(shop);
    link(modelSearchLimit, originalSearch, expandedSearch, expandedSearch);
    bytes.replace(resident->dataRomOffset + address - resident->layout.indexBase, data.size(), data);
    resident->residentChecksum = crc32(bytes.mid(resident->residentAddress - ClassPlacement::residentDelta,
                                                 resident->residentBytes));
    const auto descriptor = resident->descriptor(
        bytes.mid(resident->dataRomOffset, resident->layout.end - resident->layout.indexBase),
        bytes.mid(ClassPlacement::codeRomOffset, resident->codeBytes));
    bytes.replace(ClassPlacement::descriptorRomOffset, ClassPlacement::descriptorSize, descriptor);
    p.runtime = address;
    record(bytes, p);
}
} // namespace lordly
