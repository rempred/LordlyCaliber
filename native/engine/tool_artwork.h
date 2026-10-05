#pragma once
#include "engine/rom.h"
namespace lordly {
// Builds authored tool labels from the user's current Army UI lettering.
// Returned data is a 48x16 CI4 sheet using the War Funds palette indices.
QByteArray chaosFrameTitle(const Rom &rom);
} // namespace lordly
