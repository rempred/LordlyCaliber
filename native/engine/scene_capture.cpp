#include "engine/scene_capture.h"
#include <QPainter>
#include <cmath>
namespace lordly {
void SceneCapture::start(quint32 selector, quint32 duration, const QImage &frame) {
    selector = quint8(selector);
    duration = quint16(duration);
    if (selector >= 2 && selector <= 4 && !duration)
        throw Error("Captured-frame transition has a zero duration");
    auto captured = frame.isNull() ? QImage{} : framebuffer(frame);
    QByteArray record(16, 0);
    put8(record, 0, quint8(selector));
    put16(record, 4, quint16(duration));
    // A local identity replaces the shared target framebuffer pointer.
    put32(record, 8, captured.isNull() ? 0 : 1);
    record_ = std::move(record);
    frame_ = std::move(captured);
}
void SceneCapture::supplyRecord(QByteArray record) {
    if (!record.isEmpty() && record.size() != 16)
        throw Error("Captured-frame transition requires sixteen bytes");
    record_ = std::move(record);
    frame_ = {};
}
void SceneCapture::clear() {
    record_.clear();
    frame_ = {};
}
void SceneCapture::advance() {
    // Original 2A3FA0: unsigned halfword progress, equality (not >=) completion.
    const int kind = selector();
    if (kind < 2 || kind > 4)
        return;
    const auto progress = u16(record_, 2), duration = u16(record_, 4);
    if (kind == 4 && progress == duration)
        return;
    const auto next = quint16(progress + 1U);
    put16(record_, 2, next);
    if (kind != 4 && next == duration)
        clear();
}
quint8 SceneCapture::alpha() const {
    if (selector() < 2 || selector() > 4)
        return 0;
    const auto duration = u16(record_, 4);
    if (!duration)
        throw Error("Captured-frame transition has a zero divisor");
    return quint8(255 - int(u16(record_, 2)) * 255 / duration);
}
QImage SceneCapture::framebuffer(const QImage &source) {
    if (source.size() != QSize(320, 240))
        throw Error("Scene capture requires a 320 by 240 frame");
    QImage result(320, 240, QImage::Format_RGBA8888);
    result.fill(Qt::black);
    {
        QPainter painter(&result);
        painter.drawImage(0, 0, source);
    }
    // Preview frames have no RDP dithering; retain the target's RGBA5551 storage.
    for (int y = 0; y < 240; ++y) {
        auto p = result.scanLine(y);
        for (int x = 0; x < 320; ++x) {
            for (int c = 0; c < 3; ++c) {
                const int v = p[x * 4 + c] >> 3;
                p[x * 4 + c] = quint8((v * 255 + 15) / 31);
            }
            p[x * 4 + 3] = 255;
        }
    }
    return result;
}
std::array<SceneVignetteVertex, 10> SceneCapture::vignetteVertices() {
    // ROM29B388 called by29B5A4 with160,120. Renderer29B660 joins four
    // inner/outer pairs, then the inner quad. The +10 is before signed /2.
    return {{{-75, 65, 10},
             {-160, 108, 200},
             {-75, -55, 10},
             {-160, -108, 200},
             {85, -55, 10},
             {160, -108, 200},
             {85, 65, 10},
             {160, 108, 200},
             {-75, 65, 10},
             {-160, 108, 200}}};
}
QImage SceneCapture::vignette(const SceneCamera &camera, const QByteArray &parent) {
    const auto vertices = vignetteVertices();
    std::array<QPointF, 10> points;
    for (int i = 0; i < 10; ++i) {
        const auto point =
            camera.projectPackedPoint(parent, {double(vertices[i].x), double(vertices[i].y), 0});
        if (!point.visible)
            throw Error("Vignette projection crosses the camera clipping plane");
        points[i] = point.screen;
    }
    QImage result(320, 240, QImage::Format_RGBA8888);
    result.fill(Qt::transparent);
    auto edge = [](QPointF a, QPointF b, QPointF p) {
        return (b.x() - a.x()) * (p.y() - a.y()) - (b.y() - a.y()) * (p.x() - a.x());
    };
    auto triangle = [&](int a, int b, int c) {
        const auto p = points[a], q = points[b], r = points[c];
        const double area = edge(p, q, r);
        if (std::abs(area) < 1e-9)
            return;
        const int x0 = qBound(0, int(std::floor(qMin(p.x(), qMin(q.x(), r.x())))), 320);
        const int x1 = qBound(0, int(std::ceil(qMax(p.x(), qMax(q.x(), r.x())))), 320);
        const int y0 = qBound(0, int(std::floor(qMin(p.y(), qMin(q.y(), r.y())))), 240);
        const int y1 = qBound(0, int(std::ceil(qMax(p.y(), qMax(q.y(), r.y())))), 240);
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) {
                const QPointF pixel(x + .5, y + .5);
                const double wa = edge(q, r, pixel) / area, wb = edge(r, p, pixel) / area,
                             wc = 1 - wa - wb;
                if (wa < -1e-9 || wb < -1e-9 || wc < -1e-9)
                    continue;
                result.scanLine(y)[x * 4 + 3] =
                    quint8(qBound(0,
                                  qRound(wa * vertices[a].alpha + wb * vertices[b].alpha +
                                         wc * vertices[c].alpha),
                                  255));
            }
    };
    for (int i = 0; i < 8; i += 2) {
        triangle(i, i + 1, i + 3);
        triangle(i, i + 3, i + 2);
    }
    triangle(0, 2, 4);
    triangle(0, 4, 6);
    return result;
}
} // namespace lordly
