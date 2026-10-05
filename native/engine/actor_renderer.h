#pragma once
#include "engine/actor_instance.h"
#include "engine/actor_render_state.h"
#include "engine/class_data.h"
#include "engine/scene_image_cache.h"
#include <QCache>
#include <QImage>
namespace lordly {
enum class LayerBinding { Actor, StageFlat, StageObject };
struct LayerAuxTexture {
    const RasterImage *image;
    quint32 quarterTexelPhase;
    quint8 orientation;
};
struct ActorRasterLayer {
    int slot = 0, layer = 0, shadowIndex = 0;
    bool shadow = false;
    std::array<quint8, 4> primitive{};
    QImage image;
    CameraPlane plane;
};
// Desktop projection/composition of decoded Actor layers. The game matrix,
// primitive-color and geometry rules are retained; Qt samples and blends pixels.
// An explicit state preserves material commands and texture changes across draws.
// Convenience calls start a reference preset with transparent black environment.
// Scene callers supply equipment appearance; standalone calls use class defaults.
class ActorRenderer {
  public:
    explicit ActorRenderer(Rom rom) : rom_(std::move(rom)), frames_(64 * 1024) {}
    void setArtwork(const ActorArtworkEdits &edits) {
        if (artwork_ != edits) {
            artwork_ = edits;
            frames_.clear();
        }
    }
    QVector<ActorRasterLayer> project(const ActorInstance &, bool shadow, const SceneCamera &,
                                      const std::array<qint16, 3> &globalRgb, bool sepia = false,
                                      int shadowIndex = 0, quint8 alpha = 255,
                                      ActorRenderState *state = nullptr,
                                      SceneImageCache *images = nullptr,
                                      std::optional<BodyAppearance> appearance = {});
    QVector<ActorRasterLayer>
    projectRecord(const ActorArt &, const QByteArray &, bool shadow, const SceneCamera &,
                  const std::array<qint16, 3> &globalRgb, bool sepia = false, int shadowIndex = 0,
                  quint8 alpha = 255, ActorRenderState *state = nullptr,
                  SceneImageCache *images = nullptr, std::optional<BodyAppearance> appearance = {});
    // Stage callers retain their own combiner. Flat props bind color then mask;
    // drawable objects and Actors bind mask then color.
    static ActorRasterLayer projectLayer(const ActorFrameLayer &, const QByteArray &actor,
                                         int layer, bool shadow, const SceneCamera &,
                                         const std::array<qint16, 3> &globalRgb,
                                         double bodyMultiplier = 1., int shadowIndex = 0,
                                         quint8 alpha = 255, ActorRenderState *state = nullptr,
                                         LayerBinding binding = LayerBinding::Actor,
                                         const LayerAuxTexture *auxiliary = nullptr);
    static void paint(QImage &destination, const QVector<ActorRasterLayer> &);

  private:
    Rom rom_;
    QCache<QString, QVector<ActorFrameLayer>> frames_;
    ActorArtworkEdits artwork_;
    std::optional<ClassData> classes_;
};
} // namespace lordly
