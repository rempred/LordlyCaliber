#pragma once
#include "engine/attack_visual.h"
#include "engine/effect_machine.h"
namespace lordly {
// Mount the same visual modules exported to the game, in preview-owned RAM.
void installEffectPreviewRuntime(EffectMachine &, const AttackVisual &, int inherited);
}
