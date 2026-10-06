#pragma once
#include "engine/actor_renderer.h"
#include <QRect>
namespace lordly {
// Immutable visual inputs retained with a completed draw snapshot. Rasterize only
// the clicked pixel; ordinary repaints and selection never advance game state.
class SceneSelection {
  public:
    void actors(const QVector<ActorRasterLayer> &);
    void cover(const QVector<ActorRasterLayer> &);
    void cover(QImage, double opacity = 1.);
    void cover(quint8 alpha);
    void clip(QRect);
    int actorAt(QPoint) const;
    QRectF actorBounds(int slot) const;

  private:
    struct Entry {
        int slot = -1;
        QVector<ActorRasterLayer> layers;
        QImage image;
        double opacity = 1.;
        int alpha = -1;
        std::optional<QRect> clip;
    };
    QVector<Entry> entries_;
};
} // namespace lordly
