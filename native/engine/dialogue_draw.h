#pragma once
#include "engine/image.h"
#include <QHash>
#include <QVector>
#include <array>
namespace lordly {
struct DialogueTexture {
    int width, height, stride, bits;
    QByteArray indices;
    QVector<quint16> palette;
    bool wrapX = false, mirrorX = false, mirrorY = false;
    int maskHeight = 0;
};
struct DialogueRectangle {
    DialogueTexture texture;
    std::array<int, 4> bounds, scissor;
    std::array<double, 2> uv, step;
};
// Ordinary opaque dialogue path. Unsupported presentation modes remain explicit.
// Artwork comes from the supplied ROM; no captured drawing answers are used.
class DialogueDrawing {
  public:
    explicit DialogueDrawing(Rom rom);
    QVector<DialogueRectangle> compose(const QByteArray &window, const QByteArray &payload);
    static void paint(RasterImage &, const QVector<DialogueRectangle> &);

  private:
    QByteArray resource(quint32 key, bool compressed = false) const;
    QByteArray member(quint32 key, int index);
    DialogueTexture frame(int index);
    DialogueTexture portrait(int identity, bool variant) const;
    DialogueTexture glyph(int index, int color) const;
    Rom rom_;
    QHash<quint64, QByteArray> members_;
};
} // namespace lordly
