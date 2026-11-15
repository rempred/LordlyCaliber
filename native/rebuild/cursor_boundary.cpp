#include "rebuild/cursor_boundary.h"
#include "engine/mission_tables.h"
#include <QFile>
namespace lordly {
CursorBoundary buildCursorBoundary(const QSet<int> &keys, quint32 address, const Compiler &compiler, const QString &dir) {
    CursorBoundary p;
    p.address=address;p.keys=keys;p.bytes=QByteArray(CursorBoundary::headerSize,0);
    static_assert(MissionTables::keyCount <= CursorBoundary::keySlots);
    p.bytes.replace(0,8,"LCCURS02");
    put32(p.bytes,8,address+CursorBoundary::headerSize);put32(p.bytes,24,2);
    for (int key:keys) {
        if (key<=0 || key>=MissionTables::keyCount || key==39) throw Error("Invalid edited battlefield cursor key");
        put8(p.bytes,64+key,1);
    }
    QFile source(":/game/movement/cursor-boundary.c");
    if (!source.open(QIODevice::ReadOnly)) throw Error("Missing battlefield cursor source");
    const auto code=compiler.compileModule(source.readAll(),dir,"cursor_boundary",address+CursorBoundary::headerSize,{},
        {{"lc_cursor_boundary_keys",address+64},{"lc_cursor_key",0x8018f481},
         {"lc_cursor_selector",0x800e7ab9},{"lc_cursor_bounds",0x801f0db8},
         {"lc_cursor_flat",0x801f0dd4},{"lc_cursor_object",0x801ab850},
         {"lc_cursor_ray",0x801aa8a4},{"lc_cursor_height",0x801a63fc},
         {"lc_cursor_project",0x801a9db0},{"lc_cursor_plane",0x801aae10}},true);
    if (code.entries.value("lc_cursor_ground")!=code.address)
        throw Error("Battlefield cursor entry moved from its header");
    const auto plane=code.entries.value("lc_cursor_plane_checked");
    if (plane<code.address || plane>=code.address+code.bytes.size()) throw Error("Missing cursor-control entry");
    const quint32 offsets[]{0x1038ac,0x1130c0,0x1134b0};
    const quint32 originals[]{0x0c06aa29,0x0c06ab84,0x0c06ab84};
    for (int i=0;i<3;++i) {
        put32(p.bytes,28+i*12,offsets[i]);put32(p.bytes,32+i*12,originals[i]);
        put32(p.bytes,36+i*12,0x0c000000U|(((i?plane:code.address)>>2)&0x3ffffff));
    }
    p.bytes+=code.bytes;
    while(p.bytes.size()%16)p.bytes.append(char(0));
    if (quint64(address)+p.bytes.size()>0x80400000) throw Error("Battlefield cursor exceeds permanent storage");
    put32(p.bytes,12,p.bytes.size());put32(p.bytes,16,crc32(p.bytes));
    return p;
}
} // namespace lordly
