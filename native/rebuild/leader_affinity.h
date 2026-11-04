#pragma once
#include "engine/leader_affinity.h"
#include "rebuild/compiler.h"
namespace lordly {
struct LeaderAffinityBuild { QByteArray data, owner; };
LeaderAffinityBuild buildLeaderAffinity(const LeaderAffinity &, quint32 address,
    quint32 romOffset, const Compiler &, const QString &directory);
}
