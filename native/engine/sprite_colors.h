#pragma once
#include "engine/sprite_library.h"
#include <functional>
#include <optional>
namespace lordly {
enum class SpriteColorScope { Layer, Frame, Sequence };
// Stored RGBA, including hidden layers and RGB beneath zero alpha. Sorted by
// alpha then RGB; temporary storage is bounded by the asset's pixel budget.
std::optional<QVector<QRgb>> spriteUsedColors(const SpriteAsset &, SpriteColorScope, int frame,
                                              int layer, const std::function<bool()> &cancel = {});
} // namespace lordly
