#pragma once
#include "engine/image.h"
#include <QVector>
#include <optional>
namespace lordly {
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
};
// Native 0x5554 sprite containers retain their source headers and padded lanes.
// Child deltas are materialized independently from palette/display conversion.
class SpriteObject {
  public:
    static SpriteObject decode(QByteArray bytes);
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
    SpriteFrame frame(int requestedChild, const QByteArray &externalPalette = {},
                      bool sepia = false) const;

  private:
    SpriteLanes materialize(int child, QVector<bool> &visiting,
                            QVector<std::optional<SpriteLanes>> &cache, qsizetype &retained) const;
    QByteArray source_;
    QVector<SpriteChild> children_;
    int width_ = 0, height_ = 0, colorFormat_ = 0, alphaFormat_ = 0;
    int colorStride_ = 0, alphaStride_ = 0;
};
} // namespace lordly
