#include "engine/deleted_content.h"
#include "engine/promotion.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
namespace lordly {
namespace {
int find(const QByteArray &bytes) {
    int found=-1,at=0x2800000;
    while ((at=bytes.indexOf("LCDELETE",at))>=0) {
        const int current=at;at+=8;
        if (current%16 || current+32>bytes.size() || u32(bytes,current+12)!=quint32(current)) continue;
        if (found>=0) throw Error("Multiple deleted-content directories");
        const auto size=u32(bytes,current+16), payload=u32(bytes,current+20);
        if (u32(bytes,current+8)!=1 || size<32 || size%16 || quint64(current)+size>quint64(bytes.size()) ||
            payload>size-32 || payload>16384 || u32(bytes,current+24)!=crc32(bytes.mid(current+32,payload)) ||
            u32(bytes,current+28)!=crc32(bytes.mid(current,28))) throw Error("Damaged deleted-content directory");
        found=current;
    }
    return found;
}
}
void validateDeletedContent(const QJsonObject &value) {
    const QMap<QString,QPair<int,int>> ranges{{"classes",{165,255}},{"items",{278,510}},
        {"attacks",{159,254}},{"shops",{36,255}},{"pools",{10,31}}};
    for (auto it=value.begin();it!=value.end();++it) {
        if (!ranges.contains(it.key()) || !it.value().isArray()) throw Error("Invalid deleted-content category");
        QSet<int> seen;
        for (const auto &v:it.value().toArray()) {
            const int id=v.toInt(-1);const auto range=ranges[it.key()];
            if (!v.isDouble() || v.toDouble()!=id || id<range.first || id>range.second || seen.contains(id))
                throw Error("Invalid deleted-content ID");
            seen.insert(id);
        }
    }
}
QJsonObject readDeletedContent(const Rom &rom) {
    const auto at=find(rom.bytes());if (at<0) return {};
    const auto doc=QJsonDocument::fromJson(rom.bytes().mid(at+32,u32(rom.bytes(),at+20)));
    if (!doc.isObject()) throw Error("Invalid deleted-content directory");
    validateDeletedContent(doc.object());return doc.object();
}
QByteArray writeDeletedContent(QByteArray image,const QJsonObject &value) {
    validateDeletedContent(value);
    const int prior=find(image);
    if (prior>=0) {
        const auto size=u32(image,prior+16);
        if (prior+size==image.size()) image.resize(prior);
        else image.replace(prior,size,QByteArray(size,char(-1)));
    }
    if (value.isEmpty()) return image;
    const auto payload=QJsonDocument(value).toJson(QJsonDocument::Compact);
    while (image.size()%16) image.append(char(-1));
    const quint32 at=image.size(), size=(32+payload.size()+15)&~15;
    if (quint64(at)+size>0x4000000) throw Error("Deleted-content directory exceeds cartridge space");
    QByteArray header(32,0);header.replace(0,8,"LCDELETE");
    put32(header,8,1);put32(header,12,at);put32(header,16,size);put32(header,20,payload.size());
    put32(header,24,crc32(payload));put32(header,28,crc32(header.first(28)));
    return image+header+payload+QByteArray(size-32-payload.size(),char(-1));
}
}
