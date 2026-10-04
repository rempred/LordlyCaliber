#include "engine/scenarios.h"
#include <QSet>

namespace lordly {
namespace {
void index(int row, qsizetype size, const QString &kind) {
    if (row < 0 || row >= size)
        throw Error("Choose an existing " + kind);
}
bool nodeReference(int value, const Eset &e) {
    return value == 0 || value == 1 || value == 255 || (value >= 4 && value < 4 + e.nodes.size());
}
bool conditionReference(int value, const Eset &e) {
    return value == 0 || (value >= 1 && value <= e.conditions.size());
}
bool knownCondition(int kind) {
    return kind == 1 || kind == 4 || kind == 8 || kind == 9 || kind == 12;
}
QVector<int> nodeTargets(const QByteArray &n) {
    QVector<int> values{u8(n, 17)};
    if (u8(n, 11) == 1 || u8(n, 13) == 1)
        values << u8(n, 16);
    return values;
}
QVector<int> conditionTargets(const QByteArray &n) {
    QVector<int> values{u8(n, 10)};
    if (u8(n, 11))
        values << u8(n, 12);
    if (u8(n, 13) || u8(n, 14))
        values << u8(n, 14);
    return values;
}
} // namespace
void Scenarios::validateRoutes(const Scenario &s) const {
    const auto &e = s.layout, &old = original(s.key).layout;
    for (int i = 0; i < e.nodes.size(); ++i) {
        const auto &n = e.nodes[i];
        const auto before = old.nodes.value(i);
        if (u8(n, 0) != 4 + i)
            throw Error("Movement node IDs must follow their row order from 4 through 19");
        const bool added = before.isEmpty();
        const int kind = u8(n, 1);
        if (kind > 2 && (added || n[1] != before[1]))
            throw Error("Choose a known movement node kind");
        if (kind == 0)
            for (const int at : {2, 3})
                if (u8(n, at) > 2 && (added || n[at] != before[at]))
                    throw Error("Choose a known movement or wait order");
        if (u8(n, 11) > 3 && (added || n[11] != before[11]))
            throw Error("Choose a known gate operator");
        if (u8(n, 11) && !u8(n, 12) && (added || n.mid(10, 3) != before.mid(10, 3)))
            throw Error("A compound movement gate needs an existing secondary condition");
        // Preserve unresolved original references. Only new or changed references
        // must resolve; ordinary placement/equipment edits do not reinterpret them.
        for (const int at : {10, 12, 14}) {
            if ((at == 12 && !u8(n, 11)) || (at == 14 && !u8(n, 13) && !u8(n, 14)))
                continue;
            const bool activeBefore =
                !added &&
                (at == 10 || (at == 12 ? u8(before, 11) != 0 : u8(before, 13) || u8(before, 14)));
            if ((!activeBefore || n[at] != before[at]) && !conditionReference(u8(n, at), e))
                throw Error("Movement gate refers to a missing condition");
        }
        for (const int at : {16, 17}) {
            if (at == 16 && u8(n, 11) != 1 && u8(n, 13) != 1)
                continue;
            const bool activeBefore =
                !added && (at == 17 || u8(before, 11) == 1 || u8(before, 13) == 1);
            if ((!activeBefore || n[at] != before[at]) && !nodeReference(u8(n, at), e))
                throw Error("Movement route refers to a missing node");
        }
    }
    for (int i = 0; i < e.conditions.size(); ++i) {
        const auto &c = e.conditions[i];
        const auto before = old.conditions.value(i);
        if (u8(c, 0) != i + 1)
            throw Error("Condition IDs must follow their row order from 1 through 16");
        const int kind = u8(c, 1);
        if ((before.isEmpty() || before[1] != c[1]) && !knownCondition(kind))
            throw Error("Only understood condition kinds can be created; other original records "
                        "are preserved");
        if ((kind == 1 || kind == 8) && (before.isEmpty() || c.mid(1, 5) != before.mid(1, 5)) &&
            (u8(c, 2) > u8(c, 4) || u8(c, 3) > u8(c, 5)))
            throw Error("Condition rectangle minimum must not exceed its maximum");
        if (kind == 9 && (before.isEmpty() || c[6] != before[6]) && (u8(c, 6) < 1 || u8(c, 6) > 30))
            throw Error("Active-squad threshold must be between 1 and 30");
    }
    for (const auto &row : e.deployments) {
        QByteArray before;
        for (const auto &r : old.deployments)
            if ((u8(r, 0) & 127) == (u8(row, 0) & 127)) {
                before = r;
                break;
            }
        const int start = u8(row, 6);
        if ((before.isEmpty() || row[6] != before[6]) && !nodeReference(start, e))
            throw Error("Initial order refers to a missing movement node");
    }
    int groups = 0, members = 0, previous = 0;
    // Row zero is not a follower; subsequent rows read the preceding row's tail.
    for (int row = 1; row < e.deployments.size(); ++row) {
        const int group = u8(e.deployments[row - 1], 16);
        if (!group) {
            previous = members = 0;
            continue;
        }
        if (group != previous) {
            ++groups;
            members = 0;
            previous = group;
        }
        if (++members > 5 || groups > 10)
            throw Error("Routes support at most ten groups of five squads");
    }
}
void Scenarios::setNode(int key, int row, const QByteArray &record, const ClassData &classes) {
    auto next = scenario(key);
    index(row, next.layout.nodes.size(), "movement node");
    if (record.size() != 18 || u8(record, 0) != 4 + row)
        throw Error("Movement node identity is managed by the editor");
    next.layout.nodes[row] = record;
    replace(std::move(next), classes);
}
int Scenarios::addNode(int key, int kind, const ClassData &classes) {
    auto next = scenario(key);
    if (kind < 0 || kind > 2 || next.layout.nodes.size() >= 16)
        throw Error("Choose a known node kind in a scenario with fewer than 16 nodes");
    const int row = int(next.layout.nodes.size()), id = row + 4;
    for (const auto &n : next.layout.nodes)
        if (nodeTargets(n).contains(id))
            throw Error("An unresolved original route already uses the next node ID; change that "
                        "reference before adding a node");
    for (const auto &d : next.layout.deployments)
        if (u8(d, 6) == id)
            throw Error("An unresolved initial order already uses the next node ID; change that "
                        "order before adding a node");
    QByteArray record(18, 0);
    put8(record, 0, quint8(id));
    put8(record, 1, quint8(kind));
    record[17] = 1;
    next.layout.nodes << record;
    replace(std::move(next), classes);
    return row;
}
void Scenarios::removeNode(int key, int row, const ClassData &classes) {
    auto next = scenario(key);
    auto &e = next.layout;
    index(row, e.nodes.size(), "movement node");
    const int id = row + 4, oldEnd = int(e.nodes.size()) + 3;
    if (e.nodes.size() == 1 && !e.deployments.isEmpty())
        throw Error("A deployed scenario needs at least one movement node");
    for (const auto &d : e.deployments)
        if (u8(d, 6) == id)
            throw Error("Change squad initial orders before removing their movement node");
    for (int i = 0; i < e.nodes.size(); ++i) {
        if (i == row)
            continue;
        const auto &n = e.nodes[i];
        if (nodeTargets(n).contains(id))
            throw Error("Change incoming routes before removing this movement node");
        const int target = u8(n, 17);
        if (target > id && target <= oldEnd && target <= e.conditions.size())
            throw Error("A route value overlaps a condition ID; change that reference or remove "
                        "the final node first");
        if (row + 1 < e.nodes.size() && (u8(n, 13) || u8(n, 14) || u8(n, 15)))
            throw Error("Node removal would renumber an extended gate; resolve its payload before "
                        "removing this node");
    }
    auto remap = [id, oldEnd](QByteArray &r, int at) {
        const int value = u8(r, at);
        if (value > id && value <= oldEnd)
            put8(r, at, quint8(value - 1));
    };
    for (auto &d : e.deployments)
        remap(d, 6);
    e.nodes.removeAt(row);
    for (int i = 0; i < e.nodes.size(); ++i) {
        auto &n = e.nodes[i];
        put8(n, 0, quint8(4 + i));
        remap(n, 17);
        if (u8(n, 11) == 1)
            remap(n, 16);
    }
    replace(std::move(next), classes);
}
void Scenarios::setCondition(int key, int row, const QByteArray &record, const ClassData &classes) {
    auto next = scenario(key);
    index(row, next.layout.conditions.size(), "condition");
    if (record.size() != 10 || u8(record, 0) != row + 1)
        throw Error("Condition identity is managed by the editor");
    next.layout.conditions[row] = record;
    replace(std::move(next), classes);
}
int Scenarios::addCondition(int key, int kind, const ClassData &classes) {
    auto next = scenario(key);
    if (!knownCondition(kind) || next.layout.conditions.size() >= 16)
        throw Error(
            "Choose an understood condition kind in a scenario with fewer than 16 conditions");
    const int row = int(next.layout.conditions.size()), id = row + 1;
    for (const auto &n : next.layout.nodes)
        if (conditionTargets(n).contains(id))
            throw Error("An unresolved gate already uses the next condition ID; change that "
                        "reference before adding a condition");
    QByteArray record(10, 0);
    put8(record, 0, quint8(id));
    put8(record, 1, quint8(kind));
    if (kind == 9)
        record[6] = 1;
    next.layout.conditions << record;
    next.layout.conditionsPresent = true;
    replace(std::move(next), classes);
    return row;
}
void Scenarios::removeCondition(int key, int row, const ClassData &classes) {
    auto next = scenario(key);
    auto &e = next.layout;
    index(row, e.conditions.size(), "condition");
    // Original condition IDs can also be consumed by mission scripts outside ESET.
    if (row < original(key).layout.conditions.size())
        throw Error("Original conditions may drive mission events; edit their predicate instead of "
                    "deleting their identity");
    const int id = row + 1, oldEnd = int(e.conditions.size());
    for (const auto &n : e.nodes) {
        if (conditionTargets(n).contains(id))
            throw Error("Change movement gates before removing their condition");
        const int target = u8(n, 17);
        if (target > 1 && target >= id && target <= oldEnd)
            throw Error("A route value overlaps a removed or renumbered condition; change that "
                        "reference or remove the final condition first");
    }
    e.conditions.removeAt(row);
    if (e.conditions.isEmpty())
        e.conditionsPresent = original(key).layout.conditionsPresent;
    for (int i = 0; i < e.conditions.size(); ++i)
        put8(e.conditions[i], 0, quint8(i + 1));
    for (auto &n : e.nodes)
        for (const int at : {10, 12, 14}) {
            if ((at == 12 && !u8(n, 11)) || (at == 14 && !u8(n, 13) && !u8(n, 14)))
                continue;
            if (u8(n, at) > id && u8(n, at) <= oldEnd)
                n[at] = char(u8(n, at) - 1);
        }
    replace(std::move(next), classes);
}
void Scenarios::setRouteGroup(int key, int squadRow, int group, int pattern,
                              const ClassData &classes) {
    auto next = scenario(key);
    index(squadRow, next.squads.size(), "squad");
    if (!squadRow)
        throw Error("The first deployment does not read a preceding route-group row");
    const auto &before = next.layout.deployments[squadRow - 1];
    if (group == u8(before, 16) && (!group || pattern == u8(before, 17)))
        return;
    if (group < 0 || group > 255 || (group && !QSet<int>{1, 5, 6, 7, 8}.contains(pattern)))
        throw Error("Choose a route group label and a supported movement pattern");
    put8(next.layout.deployments[squadRow - 1], 16, quint8(group));
    put8(next.layout.deployments[squadRow - 1], 17, quint8(group ? pattern : 0));
    replace(std::move(next), classes);
}
} // namespace lordly
