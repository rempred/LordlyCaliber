#pragma once
#include "engine/rom.h"
namespace lordly {
// Restore only the authenticated authored-victory call in a temporary image.
// This lets independent whole-owner validators still check all other bytes.
QByteArray canonicalMissionImage(const Rom &);
}
