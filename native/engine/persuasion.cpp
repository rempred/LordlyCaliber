#include "engine/persuasion.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace lordly {
namespace {
constexpr quint32 resourceBase = 0x594280;
constexpr int descriptor = Persuasion::ownerSize - 32;
int integer(const QJsonValue &v, int lo, int hi) {
    const double n = v.toDouble(-9999);
    if (!v.isDouble() || !std::isfinite(n) || std::floor(n) != n || n < lo || n > hi)
        throw Error("Persuasion value is outside its supported range.");
    return int(n);
}
QJsonArray array(const QVector<int> &values) {
    QJsonArray out; for (int v : values) out << v; return out;
}
QString rankName(int rank) {
    static const QMap<int, QString> names{
        {0,"Ordinary classes"}, {10,"Beasts / Beast Masters"}, {11,"Beast Tamers"},
        {20,"Golems / Enchanters"}, {21,"Doll Masters"}, {30,"Dragons / Dragon Masters"},
        {31,"Dragon Tamers"}, {40,"Faerie / Gremlin"}, {41,"Archers / Dianas"},
        {50,"Pumpkinheads"}, {51,"Witches"}, {60,"Skeleton / Ghost / Lich"},
        {61,"Archmages"}, {70,"Vultans"}, {71,"Hawkmen"}, {72,"Ravens"},
        {80,"Goblin / Gorgon"}, {99,"Princess / Vampire"}};
    return names.value(rank, QString("Group %1").arg(rank));
}
int retailScore(int leaderRank, int targetRank) {
    if (leaderRank == 99) return 60;
    if (leaderRank == targetRank) return 80;
    return std::abs(leaderRank - targetRank) == 1 ? 60 : 30;
}
}
int Persuasion::score(int leader, int group) const {
    if (group < 0 || group >= groups_.size()) throw Error("Choose a persuasion group.");
    return rows_.value(leader, fallback_).value(group);
}
int Persuasion::groupFor(int member) const {
    for (int g = 1; g < groups_.size(); ++g)
        if (groups_[g].members.contains(member)) return g;
    return 0;
}
QVector<PersuasionGroup> Persuasion::leaderGroups() const {
    if (!leaderGroups_.isEmpty()) return leaderGroups_;
    QVector<PersuasionGroup> result;
    QSet<int> assigned;
    for (const auto &target : groups_) {
        QVector<int> members;
        for (int id : target.members) if (rows_.contains(id)) { members << id; assigned.insert(id); }
        if (!members.isEmpty()) result.append(PersuasionGroup{target.name, members});
    }
    for (int id : rows_.keys()) if (!assigned.contains(id))
        result.append(PersuasionGroup{QString("Class %1").arg(id, 2, 16, QChar('0')), {id}});
    return result;
}
void Persuasion::setGroupScore(int leaderGroup, int targetGroup, int value) {
    const auto groups = leaderGroups();
    if (leaderGroup < -1 || leaderGroup >= groups.size()) throw Error("Choose a leader group.");
    if (leaderGroup == -1) { setScore(-1, targetGroup, value); return; }
    auto next = *this;
    for (int id : groups[leaderGroup].members) next.setScore(id, targetGroup, value);
    *this = next;
}
void Persuasion::editLeaderGroup(int group, const QString &name, const QVector<int> &members, int classCount) {
    if (!unavailable_.isEmpty()) throw Error(unavailable_);
    auto next = *this;
    auto groups = leaderGroups();
    if (group < -1 || group >= groups.size() || name.trimmed().isEmpty() || name.trimmed().size() > 80 || members.isEmpty())
        throw Error("Give the leader group a name and select at least one class.");
    QSet<int> ids;
    for (int id : members) {
        if (id < 1 || id >= classCount || id > 255 || ids.contains(id)) throw Error("Choose distinct existing leader classes.");
        ids.insert(id);
        if (!next.rows_.contains(id)) next.rows_[id] = fallback_;
    }
    QVector<PersuasionGroup> result;
    for (int g = 0; g < groups.size(); ++g) {
        auto remaining = groups[g].members;
        remaining.erase(std::remove_if(remaining.begin(), remaining.end(), [&](int id) { return ids.contains(id); }), remaining.end());
        if (g == group) {
            result.append(PersuasionGroup{name.trimmed(), members});
            // Retain removed leaders' individual scores instead of silently resetting them.
            for (int id : remaining) result.append(PersuasionGroup{QString("Class %1").arg(id, 2, 16, QChar('0')), {id}});
        } else if (!remaining.isEmpty()) result.append(PersuasionGroup{groups[g].name, remaining});
    }
    if (group == -1) result.append(PersuasionGroup{name.trimmed(), members});
    for (auto &g : result) std::sort(g.members.begin(), g.members.end());
    next.leaderGroups_ = result;
    *this = next;
}
void Persuasion::setScore(int leader, int group, int value) {
    if (!unavailable_.isEmpty()) throw Error(unavailable_);
    if (group < 0 || group >= groups_.size() || value < 0 || value > 100 ||
        (leader != -1 && !rows_.contains(leader))) throw Error("Choose an existing leader row and a score from 0 to 100.");
    if (leader == -1) fallback_[group] = value; else rows_[leader][group] = value;
}
void Persuasion::addLeader(int leader) {
    if (!unavailable_.isEmpty()) throw Error(unavailable_);
    if (leader < 1 || leader > 255 || rows_.contains(leader)) throw Error("Choose a class without a leader row.");
    rows_[leader] = fallback_;
    if (!leaderGroups_.isEmpty()) leaderGroups_.append(PersuasionGroup{QString("Class %1").arg(leader, 2, 16, QChar('0')), {leader}});
}
void Persuasion::addGroup(const QString &name, const QVector<int> &members, int copyGroup, int classCount) {
    if (!unavailable_.isEmpty()) throw Error(unavailable_);
    if (groups_.size() >= 32 || copyGroup < 0 || copyGroup >= groups_.size())
        throw Error("Persuasion supports up to 32 groups; choose a group to copy its starting scores.");
    auto next = *this;
    next.leaderGroups_ = leaderGroups();
    next.groups_.append(PersuasionGroup{name, {}}); next.fallback_.append(fallback_[copyGroup]);
    for (auto it = next.rows_.begin(); it != next.rows_.end(); ++it) it.value().append(it.value()[copyGroup]);
    next.editGroup(int(next.groups_.size()) - 1, name, members, classCount);
    *this = next;
}
void Persuasion::editGroup(int group, const QString &name, const QVector<int> &members, int classCount) {
    if (!unavailable_.isEmpty()) throw Error(unavailable_);
    if (group < 1 || group >= groups_.size() || name.trimmed().isEmpty() || name.trimmed().size() > 80)
        throw Error("Give the group a name of 1–80 characters.");
    QSet<int> ids;
    for (int id : members) {
        if (id < 1 || id >= classCount || id > 255 || ids.contains(id)) throw Error("Choose distinct existing member classes.");
        ids.insert(id);
    }
    leaderGroups_ = leaderGroups();
    for (int g = 1; g < groups_.size(); ++g) {
        auto &list = groups_[g].members;
        list.erase(std::remove_if(list.begin(), list.end(), [&](int id) { return ids.contains(id); }), list.end());
    }
    groups_[group] = {name.trimmed(), members};
    std::sort(groups_[group].members.begin(), groups_[group].members.end());
}
QJsonObject Persuasion::json() const {
    QJsonArray groups; QJsonObject rows;
    for (const auto &g : groups_) groups << QJsonObject{{"name",g.name},{"members",array(g.members)}};
    for (auto it = rows_.begin(); it != rows_.end(); ++it) rows[QString::number(it.key())] = array(it.value());
    QJsonObject out{{"version",1},{"groups",groups},{"fallback",array(fallback_)},{"leaders",rows}};
    if (!leaderGroups_.isEmpty()) {
        QJsonArray leaders;
        for (const auto &g : leaderGroups_) leaders << QJsonObject{{"name",g.name},{"members",array(g.members)}};
        out["leaderGroups"] = leaders;
    }
    return out;
}
void Persuasion::apply(const QJsonObject &value, int classCount) {
    if (value == json()) return;
    if (!unavailable_.isEmpty()) throw Error(unavailable_);
    const bool namedLeaders = value.contains("leaderGroups");
    if (value.size() != (namedLeaders ? 5 : 4) || value["version"] != 1 || !value["groups"].isArray() ||
        !value["fallback"].isArray() || !value["leaders"].isObject()) throw Error("Invalid persuasion table.");
    auto next = *this; next.groups_.clear(); next.rows_.clear(); next.fallback_.clear(); next.leaderGroups_.clear();
    const auto groups = value["groups"].toArray();
    if (groups.size() < 1 || groups.size() > 32) throw Error("Persuasion requires 1–32 groups.");
    QSet<int> used;
    for (int g = 0; g < groups.size(); ++g) {
        const auto item = groups[g].toObject();
        const auto name = item["name"].toString().trimmed();
        if (item.size() != 2 || name.isEmpty() || name.size() > 80 || !item["members"].isArray())
            throw Error("Invalid persuasion group.");
        QVector<int> members;
        for (const auto &v : item["members"].toArray()) {
            const int id = integer(v, 1, std::min(255, classCount - 1));
            if (!g || used.contains(id)) throw Error("Each target class belongs to one persuasion group.");
            used.insert(id); members << id;
        }
        std::sort(members.begin(), members.end()); next.groups_.append(PersuasionGroup{name,members});
    }
    auto scores = [&](const QJsonValue &v) {
        if (!v.isArray() || v.toArray().size() != groups.size()) throw Error("A persuasion row needs one score per group.");
        QVector<int> out; for (const auto &n : v.toArray()) out << integer(n,0,100); return out;
    };
    next.fallback_ = scores(value["fallback"]);
    const auto rows = value["leaders"].toObject();
    for (auto it = rows.begin(); it != rows.end(); ++it) {
        bool ok = false; const int id = it.key().toInt(&ok);
        if (!ok || QString::number(id) != it.key() || id < 1 || id >= classCount || id > 255)
            throw Error("Persuasion references a missing leader class.");
        next.rows_[id] = scores(it.value());
    }
    if (namedLeaders) {
        const auto leaders = value["leaderGroups"].toArray();
        if (!value["leaderGroups"].isArray() || leaders.isEmpty() || leaders.size() > 255)
            throw Error("Invalid persuasion leader groups.");
        QSet<int> assigned;
        for (const auto &entry : leaders) {
            const auto item = entry.toObject(); const auto name = item["name"].toString().trimmed();
            if (item.size() != 2 || name.isEmpty() || name.size() > 80 || !item["members"].isArray() || item["members"].toArray().isEmpty())
                throw Error("Give each leader group a name and members.");
            QVector<int> members;
            for (const auto &member : item["members"].toArray()) {
                const int id = integer(member, 1, std::min(255, classCount - 1));
                if (!next.rows_.contains(id) || assigned.contains(id)) throw Error("Each leader belongs to exactly one leader group.");
                assigned.insert(id); members << id;
            }
            std::sort(members.begin(), members.end()); next.leaderGroups_.append(PersuasionGroup{name,members});
        }
        if (assigned.size() != next.rows_.size()) throw Error("Every explicit leader needs a leader group.");
    }
    *this = next;
}
QByteArray Persuasion::runtime() const {
    if (!unavailable_.isEmpty()) throw Error(unavailable_);
    if (groups_.isEmpty() || groups_.size() > 32) throw Error("Invalid persuasion groups.");
    QByteArray data(784,0);
    data.replace(0,8,"LCPERS01"); put16(data,8,groups_.size()); put16(data,10,rows_.size()+1);
    for (int id = 0; id < 256; ++id) put8(data,16+id,groupFor(id));
    for (int n : fallback_) data.append(char(n));
    for (int id = 0; id < 256; ++id) put16(data,272+id*2,784);
    for (auto it = rows_.begin(); it != rows_.end(); ++it) {
        put16(data,272+it.key()*2,data.size());
        for (int n : it.value()) data.append(char(n));
    }
    while (data.size() % 4) data.append(char(0));
    return data;
}
QByteArray Persuasion::storage() const {
    const auto data = runtime(), metadata = QJsonDocument(json()).toJson(QJsonDocument::Compact);
    QByteArray out(4,0); put32(out,0,data.size()); out += data;
    QByteArray length(4,0); put32(length,0,metadata.size()); out += length + metadata;
    while (out.size() % 4) out.append(char(0));
    return out;
}
QByteArray Persuasion::owner(quint32 offset, quint32 allocation) const {
    if (offset < 0x2800000 || quint64(offset) + allocation > 0x4000000 || allocation < storage().size())
        throw Error("Persuasion data exceeds its cartridge allocation.");
    const quint32 key = offset - resourceBase;
    QVector<quint32> words;
    QMap<QString,int> labels; QVector<QPair<int,QString>> branches;
    auto word = [&](quint32 w) { words << w; };
    auto branch = [&](quint32 w, const QString &label) { branches << qMakePair(int(words.size()),label); word(w); };
    word(0x27bdffe0); word(0xafbf001c); word(0xafb00018); word(0xafb10014);
    word(0x00808021); word(0x00a08821); // s0 leader, s1 target
    branch(0x10800000,"zero"); word(0); // no leader
    word(0x2c880100); branch(0x11000000,"zero"); word(0); // class bounds
    word(0x2ca80100); branch(0x11000000,"zero"); word(0);
    word(0x00002021); word(0x3c050000 | (key >> 16)); word(0x34a50000 | (key & 0xffff));
    word(0x0c000000 | ((0x8009dbb8u >> 2) & 0x03ffffff)); word(0);
    branch(0x10400000,"zero"); word(0x00402021); // resource pointer for release
    word(0x00514021); word(0x91080010); // member group
    word(0x00104840); word(0x01224821); word(0x95290110); // leader row offset
    word(0x01224821); word(0x01284821); word(0x91300000); // score into s0
    word(0x0c000000 | ((0x800712c4u >> 2) & 0x03ffffff)); word(0);
    branch(0x10000000,"return"); word(0x02001021);
    labels["zero"] = words.size(); word(0x00001021);
    labels["return"] = words.size();
    word(0x8fbf001c); word(0x8fb00018); word(0x8fb10014); word(0x03e00008); word(0x27bd0020);
    for (const auto &fix : branches) words[fix.first] |= quint16(labels[fix.second]-fix.first-1);
    if (words.size()*4 > descriptor) throw Error("Persuasion lookup exceeds its original code owner.");
    QByteArray out(ownerSize,0);
    for (int i=0;i<words.size();++i) put32(out,i*4,words[i]);
    out.replace(descriptor,8,"LCPERS01"); put32(out,descriptor+8,offset); put32(out,descriptor+12,allocation);
    // Full digests are validated by reconstructing canonical data and code at reopen.
    return out;
}
QByteArray Persuasion::assembly(quint32 offset, quint32 allocation) const {
    const auto bytes=owner(offset,allocation); QByteArray out(".set noreorder\n");
    for(int at=0;at<bytes.size();at+=4) out += ".word 0x" + QByteArray::number(u32(bytes,at),16) + '\n';
    return out;
}
Persuasion Persuasion::load(const Rom &rom) {
    Persuasion out;
    try {
        rom.requireSupported(); if(rom.revision()) throw Error("Persuasion editing requires US revision 0.");
        const auto code=rom.bytes().mid(ownerOffset,ownerSize);
        if(code.mid(descriptor,8)=="LCPERS01") {
            out.installedOffset_=u32(code,descriptor+8); out.installedSize_=u32(code,descriptor+12);
            const quint64 at=out.installedOffset_, allocation=out.installedSize_;
            if(at<0x2800000 || allocation>131072 || at+allocation>quint64(rom.bytes().size())) throw Error("Invalid persuasion allocation.");
            const auto stored=rom.bytes().mid(at,allocation);
            const quint32 size=u32(stored,0);
            if(size<784 || size>8976 || quint64(size)+8>allocation) throw Error("Invalid persuasion runtime table.");
            const quint32 jsonSize=u32(stored,size+4);
            if(((quint64(size)+8+jsonSize+3)&~quint64(3))!=allocation) throw Error("Invalid persuasion metadata length.");
            QJsonParseError error; const auto doc=QJsonDocument::fromJson(stored.mid(size+8,jsonSize),&error);
            if(error.error!=QJsonParseError::NoError || !doc.isObject()) throw Error("Invalid persuasion metadata.");
            out.apply(doc.object(),256);
            if(out.storage()!=stored || out.owner(out.installedOffset_,out.installedSize_)!=code)
                throw Error("Persuasion code or data failed readback.");
        } else {
            const auto resource=rom.bytes().mid(0x8b15ac,0x158);
            if(sha256(code)!="4cf7b4f85c52f6b5d27e22cc88c29713a0d6a51110f7598e339b3367ee577ebb" ||
               sha256(resource)!="48ea22f24f56ea0a475370c75a88596482c1635a413ba75f765642be624979f2")
                throw Error("Unrecognized persuasion code or data; existing ROM bytes will be preserved.");
            const auto data=resource.mid(4); QMap<int,int> ranks; QMap<int,QVector<int>> members;
            for(int at=u32(data,0);u8(data,at)!=255;at+=2) { const int id=u8(data,at),rank=u8(data,at+1); ranks[id]=rank; members[rank]<<id; }
            out.groups_.append(PersuasionGroup{"Other classes",{}}); out.fallback_<<80;
            for(auto it=members.begin();it!=members.end();++it) {
                std::sort(it.value().begin(),it.value().end());
                out.groups_.append(PersuasionGroup{rankName(it.key()),it.value()}); out.fallback_<<retailScore(255,it.key());
            }
            for(auto leader=ranks.begin();leader!=ranks.end();++leader) {
                QVector<int> row{retailScore(leader.value(),255)};
                for(auto group=members.begin();group!=members.end();++group) row<<retailScore(leader.value(),group.key());
                out.rows_[leader.key()]=row;
            }
        }
    } catch(const Error &e) { out.groups_.clear(); out.leaderGroups_.clear(); out.rows_.clear(); out.fallback_.clear(); out.unavailable_=QString::fromUtf8(e.what()); }
    out.original_=out.json(); return out;
}
}
