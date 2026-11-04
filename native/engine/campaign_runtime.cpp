#include "engine/campaign_runtime.h"
#include <QJsonDocument>
namespace lordly {
std::optional<CampaignPlacement> CampaignPlacement::read(const QByteArray &b) {
    // Follow the active loader rather than matching text inside icon names or
    // stale descriptors left behind when an allocation grows.
    qsizetype at = b.lastIndexOf("LCCAMP01");
    if(b.size()>0x5152c && (u32(b,0x51524)&0xffff0000)==0x3c040000 && (u32(b,0x51528)&0xffff0000)==0x24840000) {
        const quint32 overlay=((u32(b,0x51524)&65535)<<16)+qint16(u32(b,0x51528)&65535);
        if(overlay==sourceStart)return {};
        if(overlay<0x2800050 || quint64(overlay)>quint64(b.size()) || b.mid(overlay-80,8)!="LCCAMP01")
            throw Error("The world-map loader references an unrecognized overlay");
        at=overlay-80;
    }
    if (at < 0)
        return {};
    if (at < 0x2800000 || at + 80 > b.size() || u32(b,at+8)<1 || u32(b,at+8)>6)
        throw Error("Invalid campaign runtime descriptor");
    CampaignPlacement p;
    p.version=int(u32(b,at+8));
    p.bendCapacity=u32(b,at+8)>=4?14:7;
    const int stride=p.bendCapacity*4;
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
        quint64(p.dotAddress) + count * stride > quint64(ramStart) + p.overlaySize ||
        QByteArray::fromHex(sha256(b.mid(p.overlayOffset, p.overlaySize))) != b.mid(at + 32, 32))
        throw Error("Campaign runtime failed its bounds or integrity check");
    p.roads = b.mid(p.overlayOffset + p.roadAddress - ramStart, count * 2);
    if(p.version>=6) {
        p.roadFlags=b.mid(p.overlayOffset+codeAddress+8192-100-ramStart,count);
        for(int id=0;id<int(count);++id)
            if(u8(p.roadFlags,id)>40 || (id<40 && u8(p.roadFlags,id)!=id+1))
                throw Error("Invalid campaign road flag mapping");
    }else for(int id=0;id<int(count);++id)p.roadFlags+=char(id<40?id+1:0);
    p.dots = b.mid(p.overlayOffset + p.dotAddress - ramStart, count * stride);
    if (u32(b, p.overlayOffset + codeAddress + 8192 - ramStart) != count ||
        p.roads.first(80) != b.mid(0x858e4, 80) ||
        b.mid(p.overlayOffset + 0x864a0 - sourceStart, 320) != b.mid(0x864a0, 320))
        throw Error("Campaign runtime tables disagree with the opened ROM");
    for(int road=0;road<40;++road)
        if(p.dots.mid(road*stride,28)!=b.mid(0x86040+road*28,28))
            throw Error("Campaign first road sections disagree with the opened ROM");
    if(u32(b,at+8)>=4)for(int road=0;road<int(count);++road)for(int n=0;n<p.bendCapacity;++n) {
        const int x=u16(p.dots,road*stride+n*4),y=u16(p.dots,road*stride+n*4+2);
        if((x || y) && (x<1 || x>=512 || y<1 || y>=320))
            throw Error("Campaign road bend is outside the map");
    }
    for (int r = 0; r < int(count); ++r)
        if ((u8(p.roads,r*2) || u8(p.roads,r*2+1)) &&
            (u8(p.roads, r * 2) < 1 || u8(p.roads, r * 2) > 40 || u8(p.roads, r * 2 + 1) < 1 ||
            u8(p.roads, r * 2 + 1) > 40))
            throw Error("Campaign connection references an invalid location");
    if(u32(b,at+8)>=3) {
        const auto icons=p.overlayOffset+codeAddress+8192+4-ramStart;
        p.locationIcons=b.mid(icons,5);
        if(b.mid(icons+5,7)!=QByteArray(7,0))
            throw Error("Invalid campaign icon padding");
    }
    if(u32(b,at+8)>=2) {
        p.displayDirectory=u32(b,at+64);p.displayPoints=u32(b,at+68);
        const auto total=u32(b,at+72);
        if(p.displayDirectory!=p.dotAddress+96*stride ||
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
    if(p.version==5 || (p.version>=6 && u32(b,p.overlayOffset+codeAddress+8192-4-ramStart))) {
        const auto artAddress=u32(b,p.overlayOffset+codeAddress+8192-4-ramStart);
        const quint64 overlayEnd=quint64(ramStart)+p.overlaySize;
        if(artAddress%16 || artAddress<p.displayPoints+u32(b,at+72)*4 || quint64(artAddress)+96>overlayEnd)
            throw Error("Invalid map artwork directory");
        const auto art=p.overlayOffset+artAddress-ramStart;
        if(b.mid(art,8)!="LCMAPA01")throw Error("Missing map artwork directory");
        p.backgroundOffset=u32(b,art+8);p.backgroundAllocation=u32(b,art+12);
        const auto bg=p.backgroundOffset,allocation=p.backgroundAllocation;
        if((bg!=0x2325f76 && (bg<0x2800000 || bg%16)) || allocation<8 || allocation>0x50000 ||
           quint64(bg)+allocation>quint64(b.size()) || (bg<quint64(p.overlayOffset)+p.overlaySize && quint64(bg)+allocation>p.descriptor) ||
           u32(b,bg)+4!=allocation || u32(b,bg+4)!=512+512*320 ||
           QByteArray::fromHex(sha256(b.mid(bg,allocation)))!=b.mid(art+16,32))
            throw Error("Map background failed its allocation or integrity check");
        p.background=decodeLzss(b.mid(bg+8,allocation-8),512+512*320);
        const quint32 key=bg-0x594280;
        if(u32(b,p.overlayOffset+0x797b0-sourceStart)!=(0x3c040000|(key>>16)) ||
           u32(b,p.overlayOffset+0x797b4-sourceStart)!=(0x34840000|(key&65535)))throw Error("Map artwork loader disagrees with the directory");
        const auto records=u32(b,art+48),length=u32(b,art+92);
        if(length>100000 || records!=((artAddress+96+length+15)&~15u) || quint64(records)+64*12>overlayEnd)
            throw Error("Location icon catalog exceeds its allocation");
        QJsonParseError error;const auto document=QJsonDocument::fromJson(b.mid(art+96,length),&error);
        if(error.error!=QJsonParseError::NoError || !document.isObject())throw Error("Invalid location icon catalog encoding");
        p.artworkIcons=document.object();p.artworkAssignments=b.mid(art+52,40);
        quint32 next=records+64*12;
        for(int id=0;id<64;++id) {
            const auto slot=p.overlayOffset+records-ramStart+id*12;
            if(!p.artworkIcons.contains(QString::number(id))) {
                if(b.mid(slot,12)!=QByteArray(12,0))throw Error("Unused icon slot has data");
                continue;
            }
            const auto icon=p.artworkIcons[QString::number(id)].toObject();
            const auto frames=QByteArray::fromBase64(icon["frames"].toString().toLatin1());
            if((icon["kind"]!=0 && icon["kind"]!=1) || frames.size()!=960 || quint64(next)+960>overlayEnd ||
               u32(b,slot)!=quint32(icon["kind"].toInt()) || u32(b,slot+4)!=next || u32(b,slot+8)!=next+480 ||
               b.mid(p.overlayOffset+next-ramStart,960)!=frames)
                throw Error("Location icon runtime data disagrees with its catalog");
            next+=960;
        }
        if(next!=overlayEnd || !p.artworkIcons.contains("0") || !p.artworkIcons.contains("1"))throw Error("Invalid location icon allocation end");
        for(int n=0;n<40;++n) {
            const int id=quint8(p.artworkAssignments[n]);
            if(id>=64 || !p.artworkIcons.contains(QString::number(id)) ||
               p.artworkIcons[QString::number(id)].toObject()["kind"].toInt()!=((quint8(p.locationIcons[n/8])>>(n%8))&1))
                throw Error("Location references an invalid icon");
        }
    }
    return p;
}
} // namespace lordly
