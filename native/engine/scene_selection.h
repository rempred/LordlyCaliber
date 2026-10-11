#pragma once
#include "engine/actor_renderer.h"
#include <QRect>
namespace lordly {
enum class SceneObjectKind { Actor, Sprite };
struct SceneSelectionTarget {
    SceneObjectKind kind = SceneObjectKind::Actor;
    int slot = -1;
    explicit operator bool() const {
        return slot >= 0;
    }
    bool operator==(const SceneSelectionTarget &) const = default;
};
// Immutable visual inputs retained with a completed draw snapshot. Rasterize only
// the clicked pixel; ordinary repaints and selection never advance game state.
class SceneSelection {
  public:
    void actors(const QVector<ActorRasterLayer> &);
    void sprites(const QVector<ActorRasterLayer> &);
    void cover(const QVector<ActorRasterLayer> &);
    void cover(QImage, double opacity = 1.);
    void cover(quint8 alpha);
    void clip(QRect);
    SceneSelectionTarget objectAt(QPoint) const;
    int actorAt(QPoint) const;
    int spriteAt(QPoint) const;
    QRectF bounds(SceneSelectionTarget) const;
    QRectF actorBounds(int slot) const {
        return bounds({SceneObjectKind::Actor, slot});
    }
    QRectF spriteBounds(int slot) const {
        return bounds({SceneObjectKind::Sprite, slot});
    }

  private:
    struct Entry {
        SceneSelectionTarget target;
        QVector<ActorRasterLayer> layers;
        QImage image;
        double opacity = 1.;
        int alpha = -1;
        std::optional<QRect> clip;
    };
    QVector<Entry> entries_;
};
} // namespace lordly
