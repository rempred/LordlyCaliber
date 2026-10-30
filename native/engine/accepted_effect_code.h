#pragma once
#include "engine/effect_machine.h"
namespace lordly {
// Mount build-time compiled, byte-exact C only over unchanged, loaded ROM owners.
// Patched owners and retained assembly tails stay exactly as loaded from the ROM.
int installAcceptedEffectCode(EffectMachine &, const QByteArray &rom);
}
