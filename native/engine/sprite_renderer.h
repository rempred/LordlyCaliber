#pragma once
#include "engine/actor_renderer.h"
#include "engine/scene_sprites.h"
namespace lordly {
struct SpriteMatrices {
    QByteArray translation, rotation, scale, combined;
    // Parent is the caller's model matrix. Packed local matrices are retained;
    // host concatenation approximates the RSP's intermediate fixed precision.
    static SpriteMatrices prepare(const Rom &, const SceneSpriteDraw &, const QByteArray &parent);
};
class SpriteRenderer {
  public:
    explicit SpriteRenderer(Rom rom) : rom_(rom), renderer_(rom) {}
    QVector<ActorRasterLayer> project(const SceneSprites &, qint32 pass, quint8 special,
                                      const SceneCamera &, const QByteArray &parent,
                                      const std::array<qint16, 3> &globalRgb,
                                      const QByteArray &colorTransition = {}, bool sepia = false);

  private:
    Rom rom_;
    ActorRenderer renderer_;
};
} // namespace lordly
