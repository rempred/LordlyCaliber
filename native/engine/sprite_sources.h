#pragma once
#include "engine/actor_art.h"
#include "engine/sprite_library.h"
namespace lordly {
struct SpriteSourceFrame {
    int ticks = 8;
    QVector<ActorFrameLayer> layers;
};
// Bake decoded, oriented layers onto a common pixel grid while retaining each
// layer independently. Runtime motion, material programs and audio are not baked.
SpriteAsset spriteAssetFromFrames(const QString &, const QVector<SpriteSourceFrame> &);
SpriteAsset spriteAssetFromPose(const Rom &, const ActorArt &, int state, const QString &name,
                                int variant = 0, std::optional<BodyAppearance> appearance = {},
                                const QMap<int, QByteArray> &artwork = {});
} // namespace lordly
