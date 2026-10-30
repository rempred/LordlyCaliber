#pragma once
#include "engine/sprite_library.h"
namespace lordly {
struct EffectPiece {
    QString id, name;
    SpriteAsset artwork;
    int start = 0, duration = 30;
    QPoint from, to;
    int fromScale = 100, toScale = 100, fromAngle = 0, toAngle = 0;
    int fromOpacity = 100, toOpacity = 100, easing = 0;
    int speedPercent = 100;
    bool loop = true;
    bool operator==(const EffectPiece &) const = default;
};
struct EffectComposition {
    QString id = SpriteLibrary::newId(), name = "New effect";
    QSize size{128, 128};
    int duration = 90;
    int placement = 1; // 0 screen (legacy), 1 each target, 2 caster, 3 target side.
    QVector<EffectPiece> pieces;
    bool operator==(const EffectComposition &) const = default;
    void validate() const;
    QImage render(int tick) const;
    QJsonObject json() const;
    static EffectComposition fromJson(const QJsonObject &);
    static EffectComposition fromAsset(const SpriteAsset &);
    static bool contains(const SpriteAsset &asset) {
        return asset.provenance.contains("effectComposition");
    }
    SpriteAsset bake() const;
};
} // namespace lordly
