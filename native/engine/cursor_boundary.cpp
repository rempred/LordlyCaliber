#include "engine/cursor_boundary.h"
#include "engine/class_data.h"
#include "engine/promotion.h"
namespace lordly {
std::optional<CursorBoundary> CursorBoundary::read(const Rom &rom) {
    const auto &image = rom.bytes();
    const auto call = u32(image, 0x1038ac);
    if (call == 0x0c06aa29 || call >> 26 != 3) return {};
    const auto placement = ClassPlacement::read(rom);
    if (!placement) return {};
    const auto entry = 0x80000000U | ((call & 0x3ffffffU) << 2);
    if (entry < placement->layout.indexBase + headerSize || entry >= placement->layout.end) return {};
    const auto offset = placement->dataRomOffset + entry - placement->layout.indexBase - headerSize;
    const auto header = image.mid(offset, headerSize);
    if (!header.startsWith("LCCURS01")) return {};
    const auto size = u32(header, 12);
    if (u32(header,8) != entry || u32(header,24) != 1 || size < headerSize || size % 16 ||
        quint64(entry) - headerSize + size > placement->layout.end)
        throw Error("Invalid battlefield cursor allocation");
    CursorBoundary p;
    p.address = entry - headerSize;
    p.bytes = image.mid(offset,size);
    auto check = p.bytes;
    put32(check,16,0);
    if (crc32(check) != u32(p.bytes,16)) throw Error("Battlefield cursor checksum failed");
    for (int key=0;key<128;++key) {
        if (u8(header,64+key)>1 || ((key==0 || key==39) && u8(header,64+key)))
            throw Error("Invalid battlefield cursor map key");
        if (u8(header,64+key)) p.keys.insert(key);
    }
    const quint32 offsets[]{0x1038ac,0x1130c0,0x1134b0};
    const quint32 originals[]{0x0c06aa29,0x0c06ab84,0x0c06ab84};
    for (int i=0;i<3;++i) {
        const int at=28+i*12;
        const auto patched=u32(header,at+8),target=0x80000000U|((patched&0x3ffffffU)<<2);
        if (u32(header,at)!=offsets[i] || u32(header,at+4)!=originals[i] || patched>>26!=3 ||
            target<entry || target>=p.address+size || u32(image,offsets[i])!=patched)
            throw Error("Battlefield cursor owner differs from its compiled source");
    }
    return p;
}
QByteArray CursorBoundary::canonical(const Rom &rom) const {
    auto image=rom.bytes();
    for (int i=0;i<3;++i) put32(image,u32(bytes,28+i*12),u32(bytes,32+i*12));
    return image;
}
void CursorBoundary::install(QByteArray &image) const {
    for (int i=0;i<3;++i) {
        const auto at=u32(bytes,28+i*12);
        if (u32(image,at)!=u32(bytes,32+i*12)) throw Error("Another exporter changed the battlefield cursor query");
        put32(image,at,u32(bytes,36+i*12));
    }
    const auto opened=read(Rom::fromBytes(image));
    if (!opened || opened->bytes!=bytes) throw Error("Battlefield cursor export failed readback");
}
} // namespace lordly
