#include "engine/squad_route.h"
#include <QSet>

namespace lordly {
bool SquadRoute::extended(const QByteArray &n, const Eset *context) {
    if(u8(n, 11) > 3 || u8(n, 13) || u8(n, 14) || u8(n, 15))return true;
    const int condition=u8(n,10)-1;
    if(context && condition>=0 && condition<context->conditions.size()) {
        const int kind=u8(context->conditions[condition],1);
        // These predicates invoke mission-specific routing before the ordinary gate grammar.
        // Protect kind 20 conservatively outside its known key-54 special case too.
        if(kind==14 || kind==20)return true;
    }
    return false;
}
QVector<int> SquadRoute::targets(const QByteArray &n, const Eset *context) {
    // A zero fallback disables the whole native resolver, including branch A.
    if (!u8(n, 17)) return {};
    if (extended(n, context)) return {u8(n, 17)};
    if (u8(n, 11) == 1) return {u8(n, 16), u8(n, 17)};
    return {u8(n, 17)};
}
SquadRoute SquadRoute::read(const Scenario &s, int squad) {
    SquadRoute out;
    const auto &e = s.layout;
    if (squad < 0 || squad >= e.deployments.size()) return out;
    auto leader = [&](int row) {
        if (row > 1 && u8(e.deployments[row - 1], 16))
            while (row > 1 && e.deployments[row - 1][16] == e.deployments[row - 2][16]) --row;
        return row;
    };
    out.leader = leader(squad);
    auto traverse = [&](int start) {
        QVector<int> rows;
        QList<int> pending{start};
        while (!pending.isEmpty()) {
            const int row = pending.takeFirst() - 4;
            if (row < 0 || row >= e.nodes.size() || rows.contains(row)) continue;
            rows << row;
            const auto next = targets(e.nodes[row], &e);
            for (auto it = next.crbegin(); it != next.crend(); ++it) pending.prepend(*it);
        }
        return rows;
    };
    out.rows = traverse(u8(e.deployments[out.leader], 6));
    QSet<int> shared;
    for (int i = 0; i < e.deployments.size(); ++i) {
        if (leader(i) == out.leader) continue;
        for (int row : traverse(u8(e.deployments[leader(i)], 6))) shared.insert(row);
    }
    // Also protect entry points from currently unassigned native scripts.
    for (int i = 0; i < e.nodes.size(); ++i) {
        if (out.rows.contains(i)) continue;
        auto ids = targets(e.nodes[i], &e);
        if (extended(e.nodes[i], &e))
            for(int at:{14,15,16})ids << u8(e.nodes[i], at);
        for (int id : ids) for (int row : traverse(id)) shared.insert(row);
    }
    for (int row : out.rows) if (shared.contains(row)) out.sharedRows << row;
    return out;
}
int SquadRoute::action(const QByteArray &n) {
    const int kind = u8(n, 1), move = u8(n, 2), wait = u8(n, 3);
    if (kind == 2) return 7;
    if (kind == 0 && !move && wait <= 1) return wait;
    if (kind != 1) return -1;
    if (move == 0 && wait == 0) return int(SquadBehavior::Move);
    if (move == 0 && wait == 1) return int(SquadBehavior::MoveAndAttack);
    if (move == 1 && wait == 1) return int(SquadBehavior::HitAndRun);
    if (move == 2 && wait == 0) return int(SquadBehavior::Evade);
    if (move == 0 && wait == 2) return int(SquadBehavior::Retreat);
    return -1;
}
QString SquadRoute::actionName(const QByteArray &n) {
    const QStringList names{"Guard", "Intercept nearby enemies", "Move", "Move and attack",
                            "Hit and run", "Evade", "Retreat", "Wait dormant"};
    return names.value(action(n), "Preserved action (Advanced)");
}
QString SquadRoute::conditionName(const Eset &e, int id) {
    if (!id) return "No extra condition";
    if (id < 1 || id > e.conditions.size()) return QString("Unresolved condition %1").arg(id);
    const auto &c = e.conditions[id - 1];
    QString name;
    switch (u8(c, 1)) {
    case 1: case 8:
        name = QString("Area (%1, %2)–(%3, %4) and state")
            .arg(u8(c, 2)).arg(u8(c, 3)).arg(u8(c, 4)).arg(u8(c, 5)); break;
    case 4: name = QString("Target %1 and state").arg(u8(c, 6)); break;
    case 9: name = QString("Active squad count · threshold %1").arg(u8(c, 6)); break;
    case 12: name = QString("Site %1 state flag").arg(u8(c, 6)); break;
    case 16: name = QString("In area (%1, %2)–(%3, %4)").arg(u8(c,2)).arg(u8(c,3)).arg(u8(c,4)).arg(u8(c,5)); break;
    default: name = QString("Original event (kind %1)").arg(u8(c, 1)); break;
    }
    return QString("%1 · %2").arg(id).arg(name);
}
}
