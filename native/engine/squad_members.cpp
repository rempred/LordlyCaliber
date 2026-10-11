#include "engine/squad_members.h"
#include <QSet>
#include <algorithm>

namespace lordly {
bool independentSquad(const QByteArray &record) {
    return record.size() >= 35 && record.mid(25, 2) == QByteArray("LS", 2);
}
static int count(const QByteArray &record) {
    int result = 1;
    for (int at : {13, 14, 15, 22, 23, 24}) result += u8(record, at) != 0;
    return result;
}
QByteArray readSquadRecord(const QByteArray &library, int id) {
    if (id < 1 || qint64(id) * 35 > library.size()) throw Error("Missing squad template");
    auto record = library.mid((id - 1) * 35, 35);
    if (independentSquad(record)) {
        const int n = count(record);
        if (n < 2 || n > 5 || qint64(id - 1 + n) * 35 > library.size())
            throw Error("Independent squad members exceed their resource");
        record = library.mid((id - 1) * 35, n * 35);
    }
    squadMembers(record); // Validate extension framing before publishing it.
    return record;
}
QVector<CustomMember> squadMembers(const QByteArray &record) {
    if (record.size() < 35) throw Error("Squad template is incomplete");
    const bool extended = independentSquad(record);
    const int n = count(record);
    if (record.size() != (extended ? n * 35 : 35) || (extended && (n < 2 || n > 5)))
        throw Error("Invalid independent squad extent");
    if (extended) {
        for (int i = 1; i <= 4; ++i)
            if (u16(record, 25 + i * 2) != (i < n ? i : 0))
                throw Error("Invalid independent squad member reference");
        if (u8(record, 23) || u8(record, 24)) throw Error("Invalid independent squad formation");
        // The original constructor still chooses ordinary vs Soldier handling
        // from these cohort headers before calling the adapted member helper.
        if ((u8(record, 13) || u8(record, 14) || u8(record, 15)) &&
            record.mid(7, 6) != record.mid(35, 6))
            throw Error("Independent follower header disagrees with its member specification");
        if (u8(record, 22) && (n != 5 || record.mid(16, 6) != record.mid(140, 6)))
            throw Error("Independent final follower header is invalid");
    }
    QVector<CustomMember> result;
    for (int group : {0, 7, 16}) {
        for (int p = 0; p < (group ? 3 : 1); ++p) {
            const int cell = u8(record, group + 6 + p);
            if (group && !cell) continue;
            const int at = extended ? int(result.size()) * 35 : group;
            const int level = u8(record, at + 1);
            result << CustomMember{u8(record, at), level < 128 ? level : level - 256,
                                   cell - 1, {u16(record, at + 2), u16(record, at + 4)}};
        }
    }
    if (extended)
        for (const auto &m : result)
            if (m.classId < 2) throw Error("Independent squad members must be ordinary characters");
    return result;
}
static bool sameSpec(const CustomMember &a, const CustomMember &b) {
    return a.classId == b.classId && a.levelOffset == b.levelOffset && a.equipment == b.equipment;
}
QByteArray editSquadMembers(const QByteArray &original, const QVector<CustomMember> &members) {
    const auto before = squadMembers(original);
    if (members.isEmpty() || members.size() > 5) throw Error("Squads need one through five members");
    bool unchanged = members.size() == before.size();
    for (int i = 0; unchanged && i < members.size(); ++i)
        unchanged = sameSpec(members[i], before[i]) && members[i].cell == before[i].cell;
    if (unchanged) return original;
    for (const auto &m : members)
        if (m.classId < 1 || m.classId > 255 || m.levelOffset < -128 || m.levelOffset > 127 ||
            m.cell < 0 || m.cell > 8 || m.equipment[0] < 0 || m.equipment[0] > 65535 ||
            m.equipment[1] < 0 || m.equipment[1] > 65535)
            throw Error("Invalid squad member values");
    auto store = [&](QByteArray &r, int at, const CustomMember &m) {
        put8(r, at, quint8(m.classId)); put8(r, at + 1, quint8(m.levelOffset));
        put16(r, at + 2, quint16(m.equipment[0])); put16(r, at + 4, quint16(m.equipment[1]));
    };
    // Retain the original framing/unused fields whenever its cohorts still fit.
    if (!independentSquad(original) && members.size() == before.size()) {
        auto r = original;
        int member = 0;
        bool fits = true;
        for (int group : {0, 7, 16}) {
            int first = member;
            for (int p = 0; p < (group ? 3 : 1); ++p) {
                if (group && !u8(original, group + 6 + p)) continue;
                if (!sameSpec(members[first], members[member])) fits = false;
                store(r, group, members[member]);
                put8(r, group + 6 + p, quint8(members[member++].cell + 1));
            }
        }
        if (fits) return r;
    }
    // Keep stock encoding for any composition expressible by its two follower cohorts.
    QVector<QVector<CustomMember>> groups;
    for (int i = 1; i < members.size(); ++i) {
        if (groups.isEmpty() || !sameSpec(groups.last().first(), members[i]) || groups.last().size() == 3)
            groups << QVector<CustomMember>{};
        groups.last() << members[i];
    }
    if (groups.size() <= 2) {
        auto r = original.first(35);
        if (independentSquad(original)) r.replace(25, 10, QByteArray(10, 0));
        store(r, 0, members[0]); put8(r, 6, quint8(members[0].cell + 1));
        for (int g = 0; g < 2; ++g) {
            const int at = g ? 16 : 7;
            r.replace(at + 6, 3, QByteArray(3, 0));
            if (g >= groups.size()) continue;
            store(r, at, groups[g][0]);
            for (int p = 0; p < groups[g].size(); ++p) put8(r, at + 6 + p, quint8(groups[g][p].cell + 1));
        }
        return r;
    }
    if (!independentSquad(original) && original.mid(25, 10) != QByteArray(10, 0))
        throw Error("This squad has unknown extension data; it cannot be replaced by independent members");
    QByteArray r(members.size() * 35, 0);
    for (int i = 0; i < members.size(); ++i) {
        if (members[i].classId == 1)
            throw Error("Soldier groups retain the original cohort format; independent members use other classes");
        store(r, i * 35, members[i]);
        put8(r, i == 0 ? 6 : i < 4 ? 12 + i : 22, quint8(members[i].cell + 1));
        if (i) put16(r, 25 + i * 2, quint16(i));
    }
    store(r, 7, members[1]);
    if (members.size() == 5) store(r, 16, members[4]);
    r.replace(25, 2, "LS");
    return r;
}
void validateSquadFormation(const QVector<CustomMember> &members, const ClassData &classes) {
    if (members.isEmpty() || members.size() > 5) throw Error("Squads need one through five members");
    QSet<int> cells;
    int cost = 0;
    for (const auto &m : members) {
        if (m.classId < 1 || m.classId >= classes.entries().size()) throw Error("Choose an available class");
        if (m.cell < 0 || m.cell > 8 || cells.contains(m.cell)) throw Error("Each member needs a different formation cell");
        cells.insert(m.cell);
        const int size = u8(classes.entries()[m.classId].record, ClassData::unitSizeOffset);
        if (size > 2) throw Error("Class has an unknown unit size");
        cost += size == 2 ? 2 : 1;
        if (size == 2)
            for (const auto &other : members)
                if (&m != &other && std::abs(m.cell / 3 - other.cell / 3) <= 1 &&
                    std::abs(m.cell % 3 - other.cell % 3) <= 1)
                    throw Error("A large member needs adjacent formation cells clear");
    }
    if (cost > 5) throw Error("Squad exceeds its five formation slots");
}
}
