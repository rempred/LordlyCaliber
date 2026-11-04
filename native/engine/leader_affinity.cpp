#include "engine/leader_affinity.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include "engine/class_data.h"
#include <algorithm>
#include <cmath>

namespace lordly {
namespace {
constexpr int head = 0x5c, tail = 0x204;
// Live overlay placement verified by original callsites, not the boot mapping.
constexpr quint32 treeAddress = LeaderAffinity::address + head;
struct Lookup {
    QByteArray code;
    int table = 0;
};
Lookup lookup() {
    QVector<quint32> words;
    QMap<QString, int> labels;
    QVector<QPair<int, QString>> branches;
    auto instruction = [&](quint32 word) { words << word; };
    auto label = [&](const QString &name) { labels[name] = int(words.size()); };
    auto branch = [&](quint32 word, const QString &name) {
        branches << qMakePair(int(words.size()), name); instruction(word);
    };
    auto finish = [&] { instruction(0x08000000u | ((LeaderAffinity::address + tail) >> 2 & 0x03ffffff)); };
    instruction(0x8e04004c); // lw a0,0x4c(s0): leader's current class
    instruction(0x8e22004c); // lw v0,0x4c(s1): member's current class
    for (auto group : {std::array<int, 3>{0x45, 8, 0}, {0x4e, 3, 1}, {0x38, 13, 2}}) {
        instruction(0x24480000u | quint16(-group[0])); // addiu t0,v0,-first
        instruction(0x2d080000u | quint16(group[1]));  // sltiu t0,t0,count
        branch(0x15000000, "group");           // bne t0,zero,group
        instruction(0x24090000u | quint16(group[2]));  // delay: group column
    }
    instruction(0x2448ffcc); // member - 0x34
    instruction(0x2d080002); // Gremlin/Goblin
    branch(0x15000000, "group"); instruction(0x24090003);
    instruction(0x24080031); // Gorgon
    branch(0x10480000, "group"); instruction(0);
    instruction(0x24080037); // Ogre
    branch(0x10480000, "group"); instruction(0);
    label("zero"); finish(); instruction(0x00001021); // return zero through retail epilogue
    label("group");
    const int pointer = int(words.size());
    instruction(0); instruction(0); // lui/ori t2, table
    label("loop");
    instruction(0x91480000); // lbu t0,0(t2)
    branch(0x11000000, "zero"); instruction(0);
    branch(0x10880000, "found"); instruction(0);
    branch(0x10000000, "loop"); instruction(0x254a0005);
    label("found");
    instruction(0x01495021); // addu t2,t2,t1
    instruction(0x81420001); // lb v0,1(t2): signed strength
    finish(); instruction(0);
    Lookup result;
    result.table = int(words.size()) * 4 + 8;
    const quint32 tableAddress = treeAddress + result.table;
    words[pointer] = 0x3c0a0000u | (tableAddress >> 16);
    words[pointer + 1] = 0x354a0000u | (tableAddress & 0xffff);
    for (const auto &fix : branches)
        words[fix.first] |= quint16(labels[fix.second] - fix.first - 1);
    result.code = QByteArray(result.table, 0);
    for (int i = 0; i < words.size(); ++i) put32(result.code, i * 4, words[i]);
    result.code.replace(result.table - 8, 8, "LCAF0001");
    return result;
}
QByteArray tree(const QMap<int, LeaderAffinity::Bonuses> &rows) {
    const auto routine = lookup();
    if (rows.size() > (tail - head - lookup().table) / 5 - 1)
        throw Error(QString("Leader affinity supports %1 classes with nonzero bonuses in its original code allocation.")
                        .arg((tail - head - lookup().table) / 5 - 1));
    QByteArray result(tail - head, 0);
    result.replace(0, routine.code.size(), routine.code);
    int at = routine.table;
    for (auto row = rows.cbegin(); row != rows.cend(); ++row) {
        put8(result, at++, quint8(row.key()));
        for (int strength : row.value()) put8(result, at++, quint8(strength));
    }
    return result; // zero leader terminates; unused bytes stay zero
}
}
int LeaderAffinity::capacity() { return 255; }
LeaderAffinity::Bonuses LeaderAffinity::retail(int leader) {
    switch (leader) {
    case 0x09: return {10, 10, 10, 10};
    case 0x0a: return {0, 10, 10, 10};
    case 0x0e: return {0, 0, 10, 10};
    case 0x18: case 0x5a: return {15, 10, 10, 10};
    case 0x19: case 0x7b: return {0, 15, 10, 10};
    case 0x1d: return {0, 0, 15, 10};
    case 0x2b: case 0x36: return {0, 0, 0, 10};
    case 0x74: return {0, 0, 0, 15};
    default: return {};
    }
}
QVector<int> LeaderAffinity::members(int group) {
    QVector<int> out;
    if (group == 3) return {0x31, 0x34, 0x35, 0x37};
    const int first = group == 0 ? 0x45 : group == 1 ? 0x4e : 0x38;
    const int count = group == 0 ? 8 : group == 1 ? 3 : group == 2 ? 13 : 0;
    for (int i = 0; i < count; ++i) out << first + i;
    return out;
}
QString LeaderAffinity::groupName(int group) {
    const QStringList names{"Beasts", "Golems", "Dragons", "Other monsters"};
    return names.value(group);
}
namespace {
QVector<LeaderAffinityGroup> retailGroups() {
    QVector<LeaderAffinityGroup> groups;
    for (int i=0;i<4;++i) groups.append({LeaderAffinity::groupName(i),LeaderAffinity::members(i)});
    return groups;
}
void validateGroup(const QString &name,const QVector<int> &members,int classCount) {
    if (name.trimmed().isEmpty() || name!=name.trimmed() || name.size()>64 || name.contains(QChar(0)))
        throw Error("Use a group name of 1–64 characters");
    if(classCount<1 || classCount>256) throw Error("Invalid class count");
    QSet<int> seen;
    for(int member:members) {
        if(member<1 || member>=classCount || seen.contains(member))
            throw Error("Choose each available member class only once");
        seen.insert(member);
    }
}
}
LeaderAffinity LeaderAffinity::load(const Rom &rom) {
    LeaderAffinity out; out.groups_=retailGroups();
    try {
        rom.requireSupported();
        if (rom.revision()) throw Error("Leader affinity requires US revision 0.");
        out.owner_ = rom.bytes().mid(offset, size);
        const auto descriptor=out.owner_.mid(head+24,64);
        out.extended_=descriptor.startsWith("LCAF0002");
        if (sha256(out.owner_) == "e28eed794d8c20a6b78428bd53da9f8633c3cf8588d82ca4a8eafd8e8dd9c890") {
            for (int id = 1; id <= 255; ++id) {
                const auto values=retail(id);
                if(values!=Bonuses{})out.rows_[id]=QVector<int>(values.begin(),values.end());
            }
        } else {
            if (out.owner_.size() != size ||
                sha256(out.owner_.first(head)) != "efcd7bfcb41a6ac9e3d7c7a3292e4433ed4244cd173644ce657a02de9dfbd6ed" ||
                sha256(out.owner_.mid(tail)) != "629af1e865bef8779081c052ec164b969b4a89cd836de4b9364a637e40ce92a8")
                throw Error("Unrecognized leader-affinity code; existing ROM bytes will be preserved.");
            if(out.extended_) {
                const quint32 at=u32(descriptor,8),address=u32(descriptor,12),length=u32(descriptor,16),entry=u32(descriptor,20);
                const auto placed=ClassPlacement::read(rom);
                if(!placed || address%16 || address<placed->layout.indexBase ||
                   quint64(address)+length>placed->layout.end || length<320 || length>1024*1024 ||
                   quint64(at)+length>quint64(rom.bytes().size()) ||
                   quint64(at)!=quint64(placed->dataRomOffset)+address-placed->layout.indexBase)
                    throw Error("Combat-affinity data is outside its permanent allocation");
                const auto data=rom.bytes().mid(at,length);
                if(!data.startsWith("LCAFDT02") || u32(data,8)!=length || sha256(data)!=descriptor.mid(24,32).toHex())
                    throw Error("Damaged combat-affinity data");
                const auto jsonAt=u32(data,16),jsonBytes=u32(data,20),codeAt=u32(data,24),codeBytes=u32(data,28);
                if(jsonAt<320 || quint64(jsonAt)+jsonBytes>codeAt || codeAt%16 || codeAt>=length ||
                   !codeBytes || codeBytes>1024 || quint64(codeAt)+codeBytes!=length ||
                   entry!=quint64(address)+u32(data,32) || entry<quint64(address)+codeAt || entry>=quint64(address)+length || entry%4)
                    throw Error("Invalid combat-affinity directory");
                QJsonParseError error;const auto doc=QJsonDocument::fromJson(data.mid(jsonAt,jsonBytes),&error);
                if(error.error!=QJsonParseError::NoError || !doc.isObject())throw Error("Invalid combat-affinity group metadata");
                out.apply(doc.object());
                auto expected=out.runtimeData();put32(expected,8,length);put32(expected,28,codeBytes);put32(expected,32,entry-address);
                if(expected.size()!=codeAt || data.first(codeAt)!=expected ||
                   out.extendedOwner(at,address,data,entry)!=out.owner_)
                    throw Error("Combat-affinity code and group data disagree");
            } else {
                const auto routine=lookup();QMap<int,Bonuses> legacy;
                int at=head+routine.table,previous=0;
                while(at+5<=tail && u8(out.owner_,at)) {
                    const int id=u8(out.owner_,at++);
                    if(id<=previous)throw Error("Invalid leader-affinity table order.");
                    Bonuses values;
                    for(int &v:values) {v=qint8(u8(out.owner_,at++));if(v< -100||v>100)throw Error("Unsupported leader-affinity strength.");}
                    if(values==Bonuses{})throw Error("Invalid empty leader-affinity row.");
                    legacy[id]=values;out.rows_[id]=QVector<int>(values.begin(),values.end());previous=id;
                }
                if(out.owner_.mid(head,tail-head)!=tree(legacy))
                    throw Error("Unrecognized leader-affinity lookup; existing ROM bytes will be preserved.");
            }
        }
    } catch(const Error &e) {out.rows_.clear();out.unavailable_=QString::fromUtf8(e.what());}
    out.original_=out.json();return out;
}
LeaderAffinity::Bonuses LeaderAffinity::bonuses(int leader) const {
    Bonuses out{};for(int group=0;group<4;++group)out[group]=strength(leader,group);return out;
}
int LeaderAffinity::strength(int leader,int group) const {
    return group>=0 && group<groups_.size()?rows_.value(leader).value(group):0;
}
QList<int> LeaderAffinity::leaders(int group) const {
    QList<int> result;for(auto it=rows_.cbegin();it!=rows_.cend();++it)if(it.value().value(group)!=0)result.append(it.key());return result;
}
int LeaderAffinity::groupFor(int member) const {
    for(int group=0;group<groups_.size();++group)if(groups_[group].members.contains(member))return group;
    return -1;
}
void LeaderAffinity::set(int leader,int group,int value) {
    if(!unavailable_.isEmpty())throw Error(unavailable_);
    if(leader<1 || leader>255 || group<0 || group>=groups_.size() || value< -100 || value>100)
        throw Error("Choose a class, an affinity group and a strength from -100 to 100.");
    auto values=rows_.value(leader,QVector<int>(groups_.size(),0));values[group]=value;
    if(std::all_of(values.begin(),values.end(),[](int v){return v==0;}))rows_.remove(leader);else rows_[leader]=values;
}
void LeaderAffinity::addGroup(const QString &name,const QVector<int> &members,int classCount) {
    if(groups_.size()>=255)throw Error("All 255 combat-affinity groups are in use");
    auto next=*this;next.groups_.append(LeaderAffinityGroup{});for(auto &row:next.rows_)row.append(0);
    next.editGroup(int(next.groups_.size())-1,name,members,classCount);*this=std::move(next);
}
void LeaderAffinity::editGroup(int group,const QString &name,const QVector<int> &members,int classCount) {
    if(!unavailable_.isEmpty())throw Error(unavailable_);
    if(group<0 || group>=groups_.size())throw Error("Choose an affinity group");
    validateGroup(name,members,classCount);
    for(int i=0;i<groups_.size();++i)if(i!=group && groups_[i].name.compare(name,Qt::CaseInsensitive)==0)
        throw Error("Choose a different group name");
    auto next=groups_;
    for(auto &entry:next)for(int member:members)entry.members.removeAll(member);
    auto sorted=members;std::sort(sorted.begin(),sorted.end());next[group]={name,sorted};groups_=std::move(next);
}
void LeaderAffinity::removeGroup(int group) {
    if(!unavailable_.isEmpty())throw Error(unavailable_);
    if(group<0 || group>=groups_.size())throw Error("Choose an affinity group");
    groups_.removeAt(group);
    for(auto it=rows_.begin();it!=rows_.end();) {
        it.value().removeAt(group);
        if(std::all_of(it.value().begin(),it.value().end(),[](int v){return v==0;}))it=rows_.erase(it);else ++it;
    }
}
QJsonObject LeaderAffinity::json() const {
    QJsonObject rows;QJsonArray groups;
    for(const auto &group:groups_) {QJsonArray members;for(int id:group.members)members.append(id);groups.append(QJsonObject{{"name",group.name},{"members",members}});}
    for(auto it=rows_.cbegin();it!=rows_.cend();++it) {QJsonArray values;for(int v:it.value())values.append(v);rows[QString::number(it.key())]=values;}
    return {{"version",2},{"groups",groups},{"rows",rows}};
}
void LeaderAffinity::apply(const QJsonObject &value) {
    if(value==json())return;
    // Older projects also serialize a read-only, unrecognized owner. Accept
    // that unchanged v1 representation without attempting to replace its code.
    if(value["version"]==1 && value.size()==2 && value["rows"].isObject() &&
       groups_==retailGroups() && value["rows"]==json()["rows"])return;
    if(!unavailable_.isEmpty())throw Error(unavailable_);
    const bool legacy=value["version"]==1;
    if((!legacy && value["version"]!=2) || value.size()!=(legacy?2:3) || !value["rows"].isObject() ||
       (!legacy && (!value["groups"].isArray() || value["groups"].toArray().size()>255)))
        throw Error("Invalid leader-affinity settings.");
    auto next=*this;next.groups_=legacy?retailGroups():QVector<LeaderAffinityGroup>{};next.rows_.clear();
    if(!legacy) {
        QSet<int> used;QSet<QString> names;
        for(const auto &v:value["groups"].toArray()) {
            const auto row=v.toObject();QVector<int> members;
            if(row.size()!=2 || !row["name"].isString() || !row["members"].isArray())throw Error("Invalid affinity group");
            for(const auto &member:row["members"].toArray()) {
                const int id=member.toInt(-1);
                if(!member.isDouble() || member.toDouble()!=id || used.contains(id))throw Error("A member class can belong to only one affinity group");
                used.insert(id);members.append(id);
            }
            const auto name=row["name"].toString();validateGroup(name,members,256);
            if(names.contains(name.toCaseFolded()))throw Error("Duplicate affinity group name");names.insert(name.toCaseFolded());
            std::sort(members.begin(),members.end());next.groups_.append({name,members});
        }
    }
    const auto rows=value["rows"].toObject();
    for(auto it=rows.begin();it!=rows.end();++it) {
        bool valid=false;const int id=it.key().toInt(&valid);
        if(!valid || QString::number(id)!=it.key() || id<1 || id>255 || !it->isArray() || it->toArray().size()!=next.groups_.size())
            throw Error("Invalid leader-affinity class or group row.");
        const auto array=it->toArray();
        for(int group=0;group<array.size();++group) {
            const double v=array[group].toDouble(-999);
            if(!array[group].isDouble() || !std::isfinite(v) || std::floor(v)!=v || v< -100 || v>100)
                throw Error("Leader-affinity strength must be an integer from -100 to 100.");
            next.set(id,group,int(v));
        }
    }
    *this=std::move(next);
}
void LeaderAffinity::validateClasses(int count) const {
    if(!unavailable_.isEmpty())throw Error(unavailable_);
    for(const auto &group:groups_)validateGroup(group.name,group.members,count);
    for(auto it=rows_.begin();it!=rows_.end();++it)if(it.key()>=count)throw Error("Combat affinity references a missing leader class");
}
QByteArray LeaderAffinity::image() const {
    if(!unavailable_.isEmpty())throw Error(unavailable_);
    if(!modified())return owner_;
    if(groups_!=retailGroups())throw Error("Custom affinity groups require compiled runtime storage");
    QMap<int,Bonuses> legacy;for(auto it=rows_.cbegin();it!=rows_.cend();++it)legacy[it.key()]=bonuses(it.key());
    auto out=owner_;out.replace(head,tail-head,tree(legacy));return out;
}
QByteArray LeaderAffinity::assembly() const {
    const auto bytes=image();QByteArray out(".set noreorder\n");
    for(int at=0;at<bytes.size();at+=4)out+=".word 0x"+QByteArray::number(u32(bytes,at),16)+'\n';return out;
}
QByteArray LeaderAffinity::runtimeData() const {
    if(!unavailable_.isEmpty())throw Error(unavailable_);
    const auto metadata=QJsonDocument(json()).toJson(QJsonDocument::Compact);
    const int jsonAt=320+256*int(groups_.size());
    QByteArray bytes(jsonAt,0);bytes.replace(0,8,"LCAFDT02");put32(bytes,12,quint32(groups_.size()));
    put32(bytes,16,jsonAt);put32(bytes,20,quint32(metadata.size()));
    bytes.replace(64,256,QByteArray(256,char(-1)));
    for(int group=0;group<groups_.size();++group)for(int member:groups_[group].members)put8(bytes,64+member,quint8(group));
    for(auto row=rows_.cbegin();row!=rows_.cend();++row)for(int group=0;group<groups_.size();++group)
        put8(bytes,320+row.key()*groups_.size()+group,quint8(row.value()[group]));
    bytes+=metadata;while(bytes.size()%16)bytes.append(char(0));put32(bytes,24,quint32(bytes.size()));return bytes;
}
QByteArray LeaderAffinity::extendedOwner(quint32 romOffset,quint32 address,const QByteArray &data,quint32 entry) const {
    if(!unavailable_.isEmpty() || owner_.size()!=size)throw Error("Unknown combat-affinity owner");
    auto result=owner_;result.replace(head,tail-head,QByteArray(tail-head,0));
    // Native prologue owns saved registers and both eligibility checks.
    const quint32 words[]={0x8e04004c,0x8e25004c,0x0c000000u|((entry>>2)&0x03ffffffu),0,
        0x08000000u|(((LeaderAffinity::address+tail)>>2)&0x03ffffffu),0};
    for(int i=0;i<6;++i)put32(result,head+i*4,words[i]);
    const int at=head+24;result.replace(at,8,"LCAF0002");put32(result,at+8,romOffset);put32(result,at+12,address);
    put32(result,at+16,quint32(data.size()));put32(result,at+20,entry);result.replace(at+24,32,QByteArray::fromHex(sha256(data)));
    return result;
}
}
