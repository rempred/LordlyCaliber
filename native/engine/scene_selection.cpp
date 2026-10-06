#include "engine/scene_selection.h"
#include <QPainter>
#include <QPainterPath>
namespace lordly {
namespace {
int alphaAt(const QVector<ActorRasterLayer> &layers, QPoint point) {
    QImage pixel(1, 1, QImage::Format_RGBA8888);
    pixel.fill(Qt::transparent);
    QPainter painter(&pixel);
    for (const auto &layer : layers) {
        const auto &m = layer.plane.mapping;
        const QTransform transform(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
        if (layer.plane.clip.size() < 3 || !transform.isInvertible())
            continue;
        QPainterPath clip;
        clip.addPolygon(QPolygonF(layer.plane.clip));
        if (!clip.boundingRect().intersects(QRectF(point, QSizeF(1, 1))))
            continue;
        painter.save();
        painter.setClipPath(clip.translated(-point.x(), -point.y()));
        painter.setWorldTransform(transform * QTransform::fromTranslate(-point.x(), -point.y()));
        painter.drawImage(QPointF{}, layer.image);
        painter.restore();
    }
    painter.end();
    return pixel.pixelColor(0, 0).alpha();
}
} // namespace
void SceneSelection::actors(const QVector<ActorRasterLayer> &layers) {
    for (const auto &layer : layers)
        if (!layer.shadow)
            entries_ << Entry{{SceneObjectKind::Actor, layer.slot}, {layer}};
}
void SceneSelection::sprites(const QVector<ActorRasterLayer> &layers) {
    for (const auto &layer : layers)
        if (!layer.shadow)
            entries_ << Entry{{SceneObjectKind::Sprite, layer.slot}, {layer}};
}
void SceneSelection::cover(const QVector<ActorRasterLayer> &layers) {
    if (!layers.isEmpty())
        entries_ << Entry{{}, layers};
}
void SceneSelection::cover(QImage image, double opacity) {
    if (!image.isNull() && opacity > 0)
        entries_ << Entry{{}, {}, image.convertToFormat(QImage::Format_Alpha8), opacity};
}
void SceneSelection::cover(quint8 alpha) {
    if (alpha)
        entries_ << Entry{{}, {}, {}, 1., alpha};
}
void SceneSelection::clip(QRect rectangle) {
    entries_ << Entry{{}, {}, {}, 1., -1, rectangle};
}
SceneSelectionTarget SceneSelection::objectAt(QPoint point) const {
    if (!QRect(0, 0, 320, 240).contains(point))
        return {};
    double remaining = 1.;
    for (auto i = entries_.crbegin(); i != entries_.crend(); ++i) {
        const int alpha =
            i->clip         ? (i->clip->contains(point) ? 0 : 255)
            : i->alpha >= 0 ? i->alpha
            : !i->image.isNull()
                ? (i->image.rect().contains(point) ? i->image.pixelColor(point).alpha() : 0)
                : alphaAt(i->layers, point);
        const double coverage = qBound(0., alpha * i->opacity / 255., 1.);
        if (i->target && coverage * remaining >= 1. / 255.)
            return i->target;
        remaining *= 1. - coverage;
        if (remaining < 1. / 255.)
            return {};
    }
    return {};
}
int SceneSelection::actorAt(QPoint point) const {
    const auto target = objectAt(point);
    return target.kind == SceneObjectKind::Actor ? target.slot : -1;
}
int SceneSelection::spriteAt(QPoint point) const {
    const auto target = objectAt(point);
    return target.kind == SceneObjectKind::Sprite ? target.slot : -1;
}
QRectF SceneSelection::bounds(SceneSelectionTarget target) const {
    if (!target)
        return {};
    QRectF bounds;
    for (const auto &entry : entries_)
        if (entry.target == target)
            for (const auto &layer : entry.layers)
                bounds = bounds.united(QPolygonF(layer.plane.clip).boundingRect());
    return bounds.intersected(QRectF(0, 0, 320, 240));
}
} // namespace lordly
