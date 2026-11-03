#include "engine/personal_names.h"
#include <QJsonArray>
#include <QMap>
namespace lordly {
PersonalNames PersonalNames::retail(const Rom &rom) {
    PersonalNames result;
    QMap<quint32, int> indices;
    auto add = [&](quint32 pointer) {
        if (indices.contains(pointer)) return indices[pointer];
        const quint32 offset = pointer - 0x8012a100U;
        if (offset < 0x647f0 || offset >= 0x66e10) throw Error("Unknown personal-name pointer");
        const auto end = rom.bytes().indexOf(char(0), offset);
        if (end < offset || end - offset > 16) throw Error("Personal name exceeds character storage");
        const int id = result.names_.size();
        result.names_.append(rom.bytes().mid(offset, end-offset));
        indices[pointer] = id;
        return id;
    };
    // Include unused pool names without assuming pool order equals class ID.
    for (quint32 at=0x647f0; at<0x64a44;) {
        if (!u8(rom.bytes(), at)) { ++at; continue; }
        const int id=add(at+0x8012a100U);
        at += result.names_[id].size()+1;
    }
    for (int id=81; id<165; ++id)
        if (id<114 || id>116) result.assignments_[id]=add(u32(rom.bytes(),0x64900+id*4));
    return result;
}
int PersonalNames::assignment(int id) const {
    if (id<0 || id>=256) throw Error("Identity class is outside the name table");
    return assignments_[id];
}
void PersonalNames::assign(int id,int name) {
    assignment(id);
    if (name < -1 || name>=names_.size()) throw Error("Choose an existing personal name or Random name");
    assignments_[id]=name;
}
int PersonalNames::create(const QString &name) {
    const auto clean=name.trimmed();
    if (clean.isEmpty() || clean.size()>16) throw Error("Use 1–16 characters for a personal name");
    for (const auto ch:clean) if (ch.unicode()<32 || ch.unicode()>126)
        throw Error("Use printable game characters for a personal name");
    const auto bytes=clean.toLatin1();
    if (names_.contains(bytes)) return names_.indexOf(bytes);
    if (names_.size()>=1024) throw Error("The personal-name library is full (1024 names)");
    names_.append(bytes); return names_.size()-1;
}
void PersonalNames::rename(int id,const QString &name) {
    if(id<0 || id>=names_.size())throw Error("Unknown personal name");
    const auto clean=name.trimmed();
    if(clean.isEmpty() || clean.size()>16)throw Error("Use 1–16 characters for a personal name");
    for(auto ch:clean)if(ch.unicode()<32 || ch.unicode()>126)throw Error("Use printable game characters");
    names_[id]=clean.toLatin1();
}
QJsonObject PersonalNames::json() const {
    QJsonArray names, assignments;
    for (const auto &name:names_) names.append(QString::fromLatin1(name.toHex()));
    for (int id:assignments_) assignments.append(id);
    return {{"version",1},{"names",names},{"assignments",assignments}};
}
void PersonalNames::apply(const QJsonObject &object) {
    const auto names=object["names"].toArray(), assignments=object["assignments"].toArray();
    if (object["version"].toInt()!=1 || names.isEmpty() || names.size()>1024 || assignments.size()!=256)
        throw Error("Invalid personal-name table");
    PersonalNames next;
    for (const auto &value:names) {
        const auto hex=value.toString().toLatin1(), bytes=QByteArray::fromHex(hex);
        if (!value.isString() || bytes.toHex()!=hex || bytes.isEmpty() || bytes.size()>16 || bytes.contains(char(0)))
            throw Error("Invalid personal-name text");
        next.names_.append(bytes);
    }
    for (int id=0;id<256;++id) {
        const auto v=assignments[id];
        if (!v.isDouble() || v.toDouble()!=v.toInt(-2)) throw Error("Invalid personal-name assignment");
        next.assign(id,v.toInt());
    }
    *this=std::move(next);
}
QByteArray PersonalNames::build(quint32 address) const {
    QByteArray bytes(headerSize+256*4+names_.size()*4,0);
    bytes.replace(0,8,"LCNAMES1");put32(bytes,12,names_.size());
    QVector<quint32> pointers;
    for (int id=0;id<names_.size();++id) {
        const auto ptr=address+quint32(bytes.size());pointers.append(ptr);
        put32(bytes,headerSize+256*4+id*4,ptr);
        bytes+=names_[id]+char(0);
    }
    for (int id=0;id<256;++id) put32(bytes,pointerOffset+id*4,assignments_[id]<0?0:pointers[assignments_[id]]);
    while (bytes.size()%16) bytes.append(char(0));
    put32(bytes,8,bytes.size());return bytes;
}
PersonalNames PersonalNames::read(const QByteArray &bytes,quint32 address) {
    if (!bytes.startsWith("LCNAMES1") || bytes.size()<headerSize || u32(bytes,8)!=bytes.size() || bytes.size()%16)
        throw Error("Damaged personal-name allocation");
    const auto count=u32(bytes,12);
    if (!count || count>1024 || headerSize+256*4+count*4>bytes.size() || bytes.mid(16,16)!=QByteArray(16,0))
        throw Error("Invalid personal-name directory");
    PersonalNames result;QMap<quint32,int> ids;
    quint32 cursor=headerSize+256*4+count*4;
    for (quint32 id=0;id<count;++id) {
        const auto pointer=u32(bytes,headerSize+256*4+id*4);
        if (pointer!=address+cursor) throw Error("Personal-name pointer leaves its allocation");
        const auto end=bytes.indexOf(char(0),cursor);
        if (end<=cursor || end-cursor>16) throw Error("Invalid stored personal name");
        result.names_.append(bytes.mid(cursor,end-cursor));ids[pointer]=id;cursor=end+1;
    }
    if (bytes.mid(cursor)!=QByteArray(bytes.size()-cursor,0)) throw Error("Unexpected personal-name padding");
    for (int id=0;id<256;++id) {
        const auto ptr=u32(bytes,pointerOffset+id*4);
        if (ptr && !ids.contains(ptr)) throw Error("Personal-name assignment leaves its directory");
        result.assignments_[id]=ptr?ids[ptr]:-1;
    }
    return result;
}
void PersonalNames::verifyOwner(const Rom &rom,const QByteArray &metadata) {
    const auto expected=metadata.isEmpty()?QByteArray("5943839747d13752777f341ab173b91450a257e0eeca4221ebab781c8f161207")
                                         :metadata.mid(32,32).toHex();
    if (sha256(rom.bytes().mid(ownerOffset,ownerSize))!=expected)
        throw Error("Character naming code differs from its recognized build");
}
}
