#pragma once
#include "engine/creature_rewards.h"
#include "rebuild/compiler.h"
namespace lordly {
QByteArray rebuildCreatureRewards(const Rom &rom, const CreatureRewards &rewards,
                                  const Compiler &compiler, const QString &directory);
}
