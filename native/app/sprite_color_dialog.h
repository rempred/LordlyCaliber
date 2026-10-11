#pragma once
#include "engine/sprite_library.h"
#include <optional>
class QWidget;
namespace lordly {
std::optional<QColor> chooseSpriteUsedColor(QWidget *, const SpriteAsset &, int frame, int layer,
                                            QColor current);
}
