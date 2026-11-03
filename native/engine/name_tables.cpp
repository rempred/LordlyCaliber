#include "engine/name_tables.h"
#include "engine/text.h"
#include <QJsonArray>
#include <algorithm>
namespace lordly {
static void nameText(const QString &text,int maximum) {
    if(text.isEmpty() || text.size()>maximum)throw Error(QString("Use 1–%1 characters").arg(maximum));
    for(auto ch:text)if(ch.unicode()<32 || ch.unicode()>126)throw Error("Use printable game characters");
}
NameTables::NameTables(const Rom &rom) {
    if(rom.revision())throw Error("Name tables require US revision zero");
    for(int pool=0;pool<3;++pool) {
        const qsizetype at=quint64(0x594280)+u32(rom.bytes(),0x65dac+pool*4);
        resources_[pool]=Resource::load(rom,at,quint64(u32(rom.bytes(),at))+4);
        const auto &bytes=resources_[pool].decoded;qsizetype cursor=0;int count=256;
        if(!bytes.isEmpty() && !u8(bytes,0)) {
            if(bytes.size()<8 || bytes.first(4)!=QByteArray::fromHex("00524e31"))throw Error("Unknown expanded random-name header");
            count=int(u32(bytes,4));cursor=8;
            if(count<256 || count>randomCapacity)throw Error("Invalid random-name count");
        }
        for(int id=0;id<count;++id) {
            const auto end=bytes.indexOf(char(0),cursor);
            if(end<=cursor || end-cursor>12)throw Error("Invalid random-name list");
            const auto text=QString::fromLatin1(bytes.mid(cursor,end-cursor));nameText(text,12);
            names_[pool]<<text;cursor=end+1;
        }
        tails_[pool]=bytes.mid(cursor);
    }
    originalNames_=names_;
    for(int id=1;id<45;++id) {
        const quint32 pointer=u32(rom.bytes(),0x645cc+id*12);
        const quint32 offset=pointer-0x8012a100U;
        if(offset<0x66280 || offset>=0x665c4)throw Error("Consumable name has an unsupported pointer");
        const auto end=rom.bytes().indexOf(char(0),offset);
        if(end<offset || end>=0x665c4)throw Error("Unterminated consumable name");
        consumableOffsets_[id]=offset;consumableOriginal_[offset]=rom.bytes().mid(offset,end-offset);
        auto limit=end+1;
        while(limit<0x665c4 && !u8(rom.bytes(),limit))++limit;
        consumableSizes_[offset]=int(limit-offset);
    }
}
int NameTables::randomCount(int pool) const {
    if(pool<0 || pool>=3)throw Error("Unknown random-name table");
    return int(names_[pool].size());
}
bool NameTables::expandedRandom() const {
    return std::any_of(names_.begin(),names_.end(),[](const auto &names){return names.size()>256;});
}
QString NameTables::randomName(int pool,int id) const {
    if(id<0 || id>=randomCount(pool))throw Error("Unknown random-name entry");
    return names_[pool][id];
}
void NameTables::setRandomName(int pool,int id,const QString &text) {
    randomName(pool,id);nameText(text,12);
    names_[pool][id]=text;
}
int NameTables::addRandomName(int pool,const QString &text) {
    const int id=randomCount(pool);nameText(text,12);
    if(id>=randomCapacity)throw Error("This random-name table is full (1024 names)");
    names_[pool]<<text;return id;
}
void NameTables::removeRandomName(int pool,int id) {
    randomName(pool,id);
    if(id<256)throw Error("Original random names can be renamed but not removed");
    names_[pool].removeAt(id);
}
QString NameTables::consumableName(int id) const {
    if(!consumableOffsets_.contains(id))throw Error("Unknown consumable name");
    const auto offset=consumableOffsets_[id];return decodeText(consumableEdits_.value(offset,consumableOriginal_[offset]));
}
int NameTables::consumableCapacity(int id) const {
    consumableName(id);const auto bytes=consumableOriginal_[consumableOffsets_[id]];
    return int(decodeText(bytes).size())+consumableSizes_[consumableOffsets_[id]]-int(bytes.size())-1;
}
QVector<int> NameTables::sharingConsumableName(int id) const {
    consumableName(id);QVector<int> result;
    for(auto it=consumableOffsets_.cbegin();it!=consumableOffsets_.cend();++it)if(it.value()==consumableOffsets_[id])result<<it.key();
    return result;
}
void NameTables::setConsumableName(int id,const QString &text) {
    nameText(text,consumableCapacity(id));const auto offset=consumableOffsets_[id];
    const auto before=consumableOriginal_[offset];
    // Keep the original font controls and resident pointers, including sharing.
    auto bytes=before;int begin=0;while(begin<bytes.size() && quint8(bytes[begin])<32)++begin;
    int end=bytes.size();while(end>begin && quint8(bytes[end-1])<32)--end;
    bytes=before.first(begin)+text.toLatin1()+before.mid(end);
    if(bytes.size()>=consumableSizes_[offset])throw Error("Name exceeds its existing game allocation");
    if(bytes==before)consumableEdits_.remove(offset);else consumableEdits_[offset]=bytes;
}
QJsonObject NameTables::json() const {
    QJsonObject random,consumables;
    QJsonArray extra;bool extraChanged=false;
    for(int pool=0;pool<3;++pool) {
        for(int id=0;id<256;++id)if(names_[pool][id]!=originalNames_[pool][id])random[QString::number(pool*256+id)]=names_[pool][id];
        extra.append(QJsonArray::fromStringList(names_[pool].mid(256)));
        extraChanged|=names_[pool].mid(256)!=originalNames_[pool].mid(256);
    }
    for(auto it=consumableOffsets_.cbegin();it!=consumableOffsets_.cend();++it)
        if(consumableEdits_.contains(it.value()))consumables[QString::number(it.key())]=consumableName(it.key());
    QJsonObject result{{"version",1},{"random",random},{"consumables",consumables}};
    if(extraChanged)result["extraRandom"]=extra;
    return result;
}
void NameTables::apply(const QJsonObject &object) {
    if(object["version"].toInt()!=1 || !object["random"].isObject() || !object["consumables"].isObject())throw Error("Invalid name tables");
    auto next=*this;next.names_=originalNames_;next.consumableEdits_.clear();
    for(const auto &key:object.keys())if(key!="version" && key!="random" && key!="consumables" && key!="extraRandom")throw Error("Unknown name-table field");
    if(object.contains("extraRandom")) {
        if(!object["extraRandom"].isArray() || object["extraRandom"].toArray().size()!=3)throw Error("Invalid additional random-name tables");
        const auto pools=object["extraRandom"].toArray();
        for(int pool=0;pool<3;++pool) {
            if(!pools[pool].isArray() || pools[pool].toArray().size()>randomCapacity-256)throw Error("Invalid additional random-name count");
            next.names_[pool]=next.names_[pool].mid(0,256);
            for(const auto &name:pools[pool].toArray()) {
                if(!name.isString())throw Error("Invalid additional random name");
                next.addRandomName(pool,name.toString());
            }
        }
    }
    const auto random=object["random"].toObject(),consumables=object["consumables"].toObject();
    for(auto it=random.begin();it!=random.end();++it) {
        bool ok=false;const int id=it.key().toInt(&ok);
        if(!ok || id<0 || id>=768 || !it.value().isString())throw Error("Invalid random-name edit");
        next.setRandomName(id/256,id%256,it.value().toString());
    }
    for(auto it=consumables.begin();it!=consumables.end();++it) {
        bool ok=false;const int id=it.key().toInt(&ok);
        if(!ok || !it.value().isString())throw Error("Invalid consumable-name edit");
        next.setConsumableName(id,it.value().toString());
    }
    *this=std::move(next);
}
QByteArray NameTables::rebuild(QByteArray image,const std::function<QByteArray(const QByteArray &,const QString &)> &compile) const {
    for(int pool=0;pool<3;++pool) {
        if(names_[pool]==originalNames_[pool])continue;
        QByteArray bytes;
        if(randomCount(pool)>256) {bytes=QByteArray::fromHex("00524e3100000000");put32(bytes,4,randomCount(pool));}
        for(int id=0;id<randomCount(pool);++id)bytes+=randomName(pool,id).toLatin1()+char(0);
        bytes+=tails_[pool];bytes=compile(bytes,QString("random_names_%1").arg(pool));
        const auto encoded=encodeLzss(bytes);const auto &resource=resources_[pool];
        if(encoded.size()<=resource.capacity)image.replace(resource.romOffset,resource.originalSlot.size(),resource.container(bytes));
        else {
            const qsizetype at=(image.size()+15)&~qsizetype(15);
            if(at+encoded.size()+8>0x4000000)throw Error("Random names exceed ROM capacity");
            image.append(QByteArray(at-image.size(),char(0xff)));
            QByteArray header(8,0);put32(header,0,encoded.size()+4);put32(header,4,bytes.size());
            image+=header+encoded;while(image.size()%16)image.append(char(0xff));
            put32(image,0x65dac+pool*4,at-0x594280);
        }
    }
    for(auto it=consumableEdits_.cbegin();it!=consumableEdits_.cend();++it) {
        auto bytes=compile(it.value()+char(0),QString("consumable_name_%1").arg(it.key()));
        const int size=consumableSizes_[it.key()];
        if(bytes.size()>size)throw Error("Compiled name exceeds its allocation");
        bytes.resize(size,char(0));image.replace(it.key(),size,bytes);
    }
    return image;
}
}
