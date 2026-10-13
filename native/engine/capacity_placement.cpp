#include "engine/capacity_placement.h"
#include "engine/promotion.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace lordly {
std::optional<CapacityPlacement> CapacityPlacement::read(const Rom &rom) {
    const auto &bytes=rom.bytes();qsizetype search=0x2800000;
    std::optional<CapacityPlacement> found;
    while((search=bytes.indexOf("LC07CAPA",search))>=0) {
        const auto at=search;search+=8;
        if(at%16 || at+64>bytes.size() || u32(bytes,at+12)!=at)continue;
        if(found)throw Error("Multiple expanded-army placement records");
        const auto header=bytes.mid(at,64);
        CapacityPlacement p;
        p.offset=quint32(at);p.size=u32(header,16);p.originalSize=u32(header,20);
        p.base=u32(header,24);p.codeAddress=u32(header,28);
        const auto payloadBytes=u32(header,32);
        if(rom.revision() || u32(header,8)!=1 || p.size<64 || p.size%16 ||
            quint64(at)+p.size>quint64(bytes.size()) || p.originalSize>p.offset ||
            p.offset-p.originalSize>15 || p.base<0x80250000 || p.base%65536 ||
            p.codeAddress<p.base+0xad00 || p.codeAddress>=0x80400000 || payloadBytes>p.size-64 ||
            payloadBytes>1024*1024 || u32(header,60)!=crc32(header.first(60)) ||
            header.mid(44,16)!=QByteArray(16,0))
            throw Error("Damaged expanded-army placement record");
        const auto payload=bytes.mid(at+64,payloadBytes);
        if(crc32(payload)!=u32(header,36))throw Error("Damaged expanded-army patch record");
        const auto doc=QJsonDocument::fromJson(payload);
        if(!doc.isArray() || doc.array().size()!=u32(header,40) || doc.array().size()>16384)
            throw Error("Invalid expanded-army patch list");
        quint32 priorEnd=0;
        for(const auto v:doc.array()) {
            const auto row=v.toArray();
            if(row.size()!=3 || !row[0].isDouble() || !row[1].isString() || !row[2].isString())
                throw Error("Invalid expanded-army patch");
            const auto site=row[0].toInteger(-1);
            if(site<0 || site>=0x2800000 || row[0].toDouble()!=double(site))
                throw Error("Invalid expanded-army patch address");
            Patch patch{quint32(site),QByteArray::fromHex(row[1].toString().toLatin1()),
                QByteArray::fromHex(row[2].toString().toLatin1())};
            if(patch.site%4 || patch.site<priorEnd || patch.before.isEmpty() || patch.before.size()%4 ||
                patch.before.toHex()!=row[1].toString().toLatin1() || patch.after.toHex()!=row[2].toString().toLatin1() ||
                patch.before.size()!=patch.after.size() || quint64(patch.site)+patch.after.size()>0x2800000 ||
                bytes.mid(patch.site,patch.after.size())!=patch.after)
                throw Error("Expanded-army code differs from its recorded build");
            priorEnd=patch.site+quint32(patch.after.size());p.patches.append(patch);
        }
        found=p;
    }
    return found;
}
Rom CapacityPlacement::canonical(const Rom &rom) const {
    auto bytes=rom.bytes();
    for(const auto &patch:patches) {
        if(bytes.mid(patch.site,patch.after.size())!=patch.after)
            throw Error("Cannot restore an altered expanded-army owner");
        bytes.replace(patch.site,patch.before.size(),patch.before);
    }
    if(quint64(offset)+size==quint64(bytes.size()))bytes.resize(originalSize);
    else bytes.replace(offset,size,QByteArray(size,char(-1)));
    return Rom::fromBytes(rom.serialize(bytes),rom.sourcePath());
}
QByteArray CapacityPlacement::record(const QByteArray &before,const QByteArray &after,quint32 base,quint32 codeAddress) {
    if(before.size()!=after.size() || before.size()<0x2800000 || before.mid(0x2800000)!=after.mid(0x2800000))
        throw Error("Capacity patch record must preserve all resource allocations");
    QJsonArray patches;
    for(quint32 at=0;at<0x2800000;) {
        if(u32(before,at)==u32(after,at)){at+=4;continue;}
        const auto start=at;
        do {at+=4;}while(at<0x2800000 && u32(before,at)!=u32(after,at));
        patches.append(QJsonArray{double(start),QString::fromLatin1(before.mid(start,at-start).toHex()),
            QString::fromLatin1(after.mid(start,at-start).toHex())});
    }
    const auto payload=QJsonDocument(patches).toJson(QJsonDocument::Compact);
    auto image=after;while(image.size()%16)image.append(char(-1));
    const auto at=quint32(image.size()),size=(64U+quint32(payload.size())+15U)&~15U;
    if(quint64(at)+size>0x4000000)throw Error("Capacity metadata exceeds cartridge space");
    QByteArray header(64,0);header.replace(0,8,"LC07CAPA");
    put32(header,8,1);put32(header,12,at);put32(header,16,size);put32(header,20,quint32(before.size()));
    put32(header,24,base);put32(header,28,codeAddress);put32(header,32,quint32(payload.size()));
    put32(header,36,crc32(payload));put32(header,40,quint32(patches.size()));put32(header,60,crc32(header.first(60)));
    image+=header+payload+QByteArray(size-64-payload.size(),char(-1));
    return image;
}
} // namespace lordly
