#include "engine/campaign_runtime.h"
namespace lordly {
std::optional<CampaignPlacement> CampaignPlacement::read(const QByteArray &b) {
    const auto at = b.lastIndexOf("LCCAMP01");
    if (at < 0)
        return {};
    if (at < 0x2800000 || at + 80 > b.size() || u32(b,at+8)<1 || u32(b,at+8)>3)
        throw Error("Invalid campaign runtime descriptor");
    CampaignPlacement p;
    p.descriptor = quint32(at);
    p.overlayOffset = u32(b, at + 12);
    p.overlaySize = u32(b, at + 16);
    const auto count = u32(b, at + 20);
    p.roadAddress = u32(b, at + 24);
    p.dotAddress = u32(b, at + 28);
    if (count < 40 || count > 96 || p.overlayOffset != quint32(at + 80) ||
        p.overlaySize < sourceEnd - sourceStart ||
        quint64(p.overlayOffset) + p.overlaySize > quint64(b.size()) ||
        ramStart + p.overlaySize > 0x801f1510 || p.roadAddress != codeAddress + 8192 + 16 ||
        p.dotAddress != p.roadAddress + 192 ||
        quint64(p.dotAddress) + count * 28 > quint64(ramStart) + p.overlaySize ||
        QByteArray::fromHex(sha256(b.mid(p.overlayOffset, p.overlaySize))) != b.mid(at + 32, 32))
        throw Error("Campaign runtime failed its bounds or integrity check");
    p.roads = b.mid(p.overlayOffset + p.roadAddress - ramStart, count * 2);
    p.dots = b.mid(p.overlayOffset + p.dotAddress - ramStart, count * 28);
    if (u32(b, p.overlayOffset + codeAddress + 8192 - ramStart) != count ||
        p.roads.first(80) != b.mid(0x858e4, 80) || p.dots.first(1120) != b.mid(0x86040, 1120) ||
        b.mid(p.overlayOffset + 0x864a0 - sourceStart, 320) != b.mid(0x864a0, 320))
        throw Error("Campaign runtime tables disagree with the opened ROM");
    for (int r = 0; r < int(count); ++r)
        if ((u8(p.roads,r*2) || u8(p.roads,r*2+1)) &&
            (u8(p.roads, r * 2) < 1 || u8(p.roads, r * 2) > 40 || u8(p.roads, r * 2 + 1) < 1 ||
            u8(p.roads, r * 2 + 1) > 40))
            throw Error("Campaign connection references an invalid location");
    if(u32(b,at+8)>=3) {
        const auto icons=p.overlayOffset+codeAddress+8192+4-ramStart;
        p.locationIcons=b.mid(icons,5);
        if(b.mid(icons+5,7)!=QByteArray(7,0))throw Error("Invalid campaign icon padding");
    }
    if(u32(b,at+8)>=2) {
        p.displayDirectory=u32(b,at+64);p.displayPoints=u32(b,at+68);
        const auto total=u32(b,at+72);
        if(p.displayDirectory!=p.dotAddress+96*28 ||
            p.displayPoints!=p.displayDirectory+400 || total>96*512 ||
            quint64(p.displayPoints)+total*4>quint64(ramStart)+p.overlaySize ||
            b.mid(p.overlayOffset+0x86040-sourceStart,1120)!=QByteArray(1120,0))
            throw Error("Campaign display dots exceed their allocation");
        const auto directory=p.overlayOffset+p.displayDirectory-ramStart;
        const auto points=p.overlayOffset+p.displayPoints-ramStart;
        quint32 begin=0;
        if(u32(b,directory))throw Error("Invalid first campaign display offset");
        for(int r=0;r<int(count);++r) {
            const auto end=u32(b,directory+(r+1)*4);
            if(end<begin || end>total || end-begin>512 || (!u8(p.roads,r*2) && end!=begin))
                throw Error("Invalid campaign display dot range");
            QVector<QPoint> dots;
            for(auto n=begin;n<end;++n) {
                const QPoint point(u16(b,points+n*4),u16(b,points+n*4+2));
                if(point.x()<1 || point.x()>=512 || point.y()<1 || point.y()>=320)
                    throw Error("Campaign display dot is outside the map");
                dots<<point;
            }
            p.displayDots<<dots;begin=end;
        }
        if(begin!=total)throw Error("Unused campaign display dot data");
        const auto names=u32(b,at+76);
        if(names) {
            if(names%16 || names<quint64(p.displayPoints)+total*4 || quint64(names)+160>quint64(ramStart)+p.overlaySize)
                throw Error("Map location names exceed their allocation");
            const auto directory=p.overlayOffset+names-ramStart;
            for(int id=1;id<=40;++id) {
                const auto pointer=u32(b,directory+(id-1)*4);
                if(!pointer)continue;
                if(pointer<names+160 || quint64(pointer)+3>quint64(ramStart)+p.overlaySize ||
                    u32(b,0x864a0+(id-1)*8+4)!=pointer)
                    throw Error("Invalid map location name pointer");
                const auto start=p.overlayOffset+pointer-ramStart;
                const auto end=b.indexOf(char(0),start);
                if(end<0 || end-start<3 || end-start>50 || quint64(end)>=quint64(p.overlayOffset)+p.overlaySize ||
                    u8(b,start)!=14 || u8(b,end-1)!=15)
                    throw Error("Invalid map location name text");
                const auto text=b.mid(start+1,end-start-2);
                for(auto ch:text)if(quint8(ch)<32 || quint8(ch)>126)throw Error("Invalid map location name character");
                p.locationNames[id]=QString::fromLatin1(text);
            }
        }
    }
    return p;
}
} // namespace lordly
