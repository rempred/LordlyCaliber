#pragma once
#include "engine/sprite_library.h"
#include <optional>
namespace lordly {
struct EffectArtwork {
    int width = 0, height = 0;
    QByteArray rgba, originalHash;
    bool operator==(const EffectArtwork &) const = default;
};
struct AttackVisual {
    int nativeEffect = -1, delay = 0;
    std::optional<SpriteAsset> frames;
    int hue = 0, color = -1, sizePercent = 100, offsetX = 0, offsetY = 0;
    QMap<quint32, EffectArtwork> artwork; // Native resource type:ID, independently copied.
    bool nativeEdited() const {
        return hue || color >= 0 || sizePercent != 100 || offsetX || offsetY || !artwork.isEmpty();
    }
    bool operator==(const AttackVisual &) const = default;
    bool modified() const {
        return nativeEffect != -1 || delay || frames.has_value() || nativeEdited();
    }
    void validate() const;
    QJsonObject json() const;
    static AttackVisual fromJson(const QJsonObject &);
    QByteArray build(int inheritedEffect) const;
    static AttackVisual read(const QByteArray &);
    static QByteArray gameImage(const QImage &);
};
} // namespace lordly
