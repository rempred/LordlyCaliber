#pragma once
#include "engine/attack_visual.h"
namespace lordly {
QByteArray retailEffectResource(const Rom &, int type, int id);
void validateEffectArtwork(const Rom &, const AttackVisual &);
}
