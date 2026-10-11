#pragma once
#include "engine/actor_renderer.h"
#include "engine/stage_perspective.h"
namespace lordly {
class StageObjectRenderer {
  public:
    explicit StageObjectRenderer(Rom rom) : rom_(std::move(rom)), frames_(64 * 1024) {}
    static QByteArray modelMatrix(const Rom &, const QByteArray &drawing,
                                  const SceneCamera &worldCamera, const QByteArray &screenParent);
    void setArtwork(const ActorArtworkEdits &edits) {
        if (artwork_ != edits) {
            artwork_ = edits;
            frames_.clear();
        }
    }
    QVector<ActorRasterLayer> project(const StageWorldProp &, int slot, const SceneCamera &world,
                                      const SceneCamera &screen, const QByteArray &screenParent,
                                      const std::array<qint16, 3> &globalRgb, ActorRenderState &,
                                      std::optional<RenderColor> transition = {},
                                      quint32 texturePhase = 0, bool sepia = false);

  private:
    Rom rom_;
    QCache<QString, QVector<ActorFrameLayer>> frames_;
    ActorArtworkEdits artwork_;
    std::optional<ClassData> classes_;
    std::optional<RasterImage> auxiliary_;
};
} // namespace lordly
