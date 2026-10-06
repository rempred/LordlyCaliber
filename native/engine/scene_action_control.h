#pragma once
#include "engine/director.h"
namespace lordly::action_detail {
struct CounterState {
    bool known = true;
    int arm = -1;
    qint64 target = 0;
};
QVector<CounterState> counterStates(const DirectorProgram &);
QVector<int> remainingGates(const DirectorProgram &, int before, qint64 previous);
void shiftGates(DirectorProgram &, const QVector<int> &, qint64 delta);
int setupEnd(const DirectorProgram &);
} // namespace lordly::action_detail
