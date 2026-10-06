#pragma once
#include "engine/image.h"
#include <QMap>
#include <QVector>
#include <optional>
namespace lordly {
// Visible pixels retain native channel values; I4 alpha is 0..15, I8 is 0..255.
struct SpritePixels {
    QByteArray color, alpha;
    bool operator==(const SpritePixels &) const = default;
};
struct SpriteLanes {
    QByteArray color, alpha, palette;
};
struct SpriteChild {
    int discriminator = 0, flags = 0, widthField = 0, heightField = 0;
    qsizetype colorOffset = -1, alphaOffset = -1, paletteOffset = -1;
};
struct SpriteFrame {
    RasterImage image;
    int requestedChild = 0, child = 0;
    // Original color alpha survives the separate transparency lane. The Actor
    // callback binds transparency to tile0 and RGBA16 color to tile1.
    QByteArray colorAlpha;
    bool dualTexture = false;
    int sourceColorFormat = 2;
    SpriteFrame oriented(int width, int height, bool flipX, bool flipY) const;
};
// Native 0x5554 sprite containers retain their source headers and padded lanes.
// Child deltas are materialized independently from palette/display conversion.
class SpriteObject {
  public:
    static SpriteObject decode(QByteArray bytes);
    static SpriteObject fromImage(const RasterImage &, bool orderedDither = false);
    const QByteArray &source() const {
        return source_;
    }
    int width() const {
        return width_;
    }
    int height() const {
        return height_;
    }
    int colorFormat() const {
        return colorFormat_;
    }
    int alphaFormat() const {
        return alphaFormat_;
    }
    const QVector<SpriteChild> &children() const {
        return children_;
    }
    SpriteLanes materialize(int child) const;
    SpritePixels pixels(int child) const;
    QByteArray rebuild(const QMap<int, SpritePixels> &edits) const;
    SpriteFrame frame(int requestedChild, const QByteArray &externalPalette = {}, int sepia = 0,
                      bool warmTone = false) const;

  private:
    SpriteLanes materialize(int child, QVector<bool> &visiting,
                            QVector<std::optional<SpriteLanes>> &cache, qsizetype &retained) const;
    QByteArray source_;
    QVector<SpriteChild> children_;
    int width_ = 0, height_ = 0, colorFormat_ = 0, alphaFormat_ = 0;
    int colorStride_ = 0, alphaStride_ = 0;
};
} // namespace lordly
