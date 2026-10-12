#pragma once
#include "engine/scenarios.h"

namespace lordly {
// Presentation order only. Native node identities and branch edges are unchanged.
struct SquadRoute {
    int leader = -1;
    QVector<int> rows;
    QVector<int> sharedRows;
    static SquadRoute read(const Scenario &, int squad);
    int number(int nodeId) const { return rows.indexOf(nodeId - 4) + 1; }
    static QVector<int> targets(const QByteArray &, const Eset *context = nullptr);
    static bool extended(const QByteArray &, const Eset *context = nullptr);
    static int action(const QByteArray &); // SquadBehavior, 7 = dormant, -1 = preserved
    static QString actionName(const QByteArray &);
    static QString conditionName(const Eset &, int id);
};
}
