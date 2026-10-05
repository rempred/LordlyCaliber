#pragma once
#include "engine/actor_renderer.h"
#include "engine/scene_actors.h"
#include "engine/scene_effects.h"
namespace lordly {
struct SceneActorRenderContext {
    SceneCamera camera;
    std::array<qint16, 3> globalRgb;
    RenderColor environment;
    bool sepia = false;
    SceneEffects *effects = nullptr;
    std::optional<quint8> bodyPaletteReplacement;
};
struct SceneActorFrame {
    QImage image;
    QVector<ActorRasterLayer> layers;
    RenderColor environment;
};
// Composes already prepared matrices in the native pass order. Matrix/terrain
// preparation, equipment placement and scene-object geometry have separate owners.
class SceneActorRenderer {
  public:
    explicit SceneActorRenderer(Rom rom) : renderer_(std::move(rom)) {}
    // Object callbacks paint into a private frame and retain their material state.
    // They must not mutate external state; an error discards the frame and Actor edits.
    using ObjectRender = std::function<void(int kind, int index, QImage &, ActorRenderState &)>;
    SceneActorFrame layered(SceneActors &, const SceneDrawQueue &, quint8 objectChannel,
                            quint8 channel, const SceneActorRenderContext &,
                            const ObjectRender &objects = {});
    SceneActorFrame world(SceneActors &, const SceneDrawQueue &, const QVector<QByteArray> &objects,
                          const std::array<qint16, 2> &shadowAlpha,
                          const std::array<quint8, 6> &gradient, const SceneActorRenderContext &,
                          const ObjectRender &drawObject = {});
    // ROM2ADAF4: black rectangle, then first eligible Actor in slot order.
    // Its callbacks retain main geometry and do not advance tint/material jobs.
    SceneActorFrame special(const SceneActors &, SceneEffects &,
                            const std::array<quint8, 6> &gradient, const SceneActorRenderContext &);

  private:
    SceneActorFrame draw(SceneActors &, const QVector<LordlySceneWorldDrawPass> &,
                         const SceneActorRenderContext &, const ObjectRender &,
                         const std::optional<std::array<quint8, 6>> &gradient);
    ActorRenderer renderer_;
};
} // namespace lordly
