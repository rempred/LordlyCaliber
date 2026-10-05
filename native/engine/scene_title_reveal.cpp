#include "engine/scene_title_reveal.h"
#include "game/director/title_reveal.h"
#include <QPainter>
namespace lordly {
void SceneTitleReveal::start(const QVector<quint32> &args) {
    if (args.size() != 6)
        throw Error("Title reveal requires six operands");
    QByteArray next(14, 0);
    lordly_title_start(reinterpret_cast<uint8_t *>(next.data()), args.constData());
    record_ = std::move(next);
}
void SceneTitleReveal::advance() {
    if (!record_.isEmpty() && !lordly_title_update(reinterpret_cast<uint8_t *>(record_.data())))
        record_.clear();
}
bool SceneTitleReveal::active() const {
    return !record_.isEmpty() && u16(record_, 8) != u16(record_, 10);
}
std::array<quint32, 8> SceneTitleReveal::rectangles(bool high) const {
    std::array<quint32, 8> result{};
    if (!record_.isEmpty() &&
        !lordly_title_rectangles(reinterpret_cast<const uint8_t *>(record_.constData()), high,
                                 result.data()))
        throw Error("Title reveal has a zero duration while drawing (native division by zero)");
    return result;
}
QImage SceneTitleReveal::render(bool high) const {
    QImage frame(high ? 640 : 320, high ? 480 : 240, QImage::Format_RGBA8888);
    frame.fill(Qt::transparent);
    if (!record_.isEmpty()) {
        const auto words = rectangles(high);
        QPainter p(&frame);
        // Title wrapper ROM281E28..281E98 selects this scissor. The reveal's
        // prefix at ROM299EF8/live8023DFB8 selects opaque black primitive color
        // and the primitive-only FCFFFFFF/FFFDF6FB combiner in both paths.
        // The high-resolution CI4 tile is loaded but cannot affect this color.
        p.setClipRect(high ? QRect(0, 0, 640, 480) : QRect(0, 0, 319, 239));
        for (int i : {0, 4}) {
            const int left = (words[i + 1] >> 12 & 0xfff) / 4;
            const int top = (words[i + 1] & 0xfff) / 4;
            const int right = (words[i] >> 12 & 0xfff) / 4;
            const int bottom = (words[i] & 0xfff) / 4;
            if (right > left && bottom > top)
                p.fillRect(QRect(left, top, right - left, bottom - top), Qt::black);
        }
    }
    return high ? frame.scaled(320, 240, Qt::IgnoreAspectRatio, Qt::FastTransformation) : frame;
}
} // namespace lordly
