#pragma once
#include "engine/sprite_library.h"
#include <optional>
namespace lordly {
struct AttackVisual {
    int nativeEffect = -1, delay = 0;
    std::optional<SpriteAsset> frames;
    bool operator==(const AttackVisual &) const = default;
    bool modified() const {
        return nativeEffect != -1 || delay || frames.has_value();
    }
    void validate() const;
    QJsonObject json() const;
    static AttackVisual fromJson(const QJsonObject &);
    QByteArray build(int inheritedEffect) const;
    static AttackVisual read(const QByteArray &);
    static QByteArray gameImage(const QImage &);
};
} // namespace lordly
