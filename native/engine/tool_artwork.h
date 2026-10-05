#pragma once
#include "engine/rom.h"
namespace lordly {
// Builds authored tool labels from the user's current Army UI lettering.
// Returned data is a 48x16 CI4 sheet using the War Funds palette indices.
QByteArray chaosFrameTitle(const Rom &rom);
enum class ToolLabel { Luck, Alignment };
// 64x16 CI4: native label in row 0, ten six-pixel digit cells in row 8.
QByteArray displayToolTexture(const Rom &rom, ToolLabel label);
} // namespace lordly
