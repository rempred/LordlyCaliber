#pragma once
#include "engine/actor_renderer.h"
#include "engine/direct_pose.h"
#include "engine/stage_placements.h"
namespace lordly {
struct StageFlatProp {
    QByteArray header; // First20 bytes of the native100-byte record.
    DirectPosePlayer pose;
    QByteArray record() const {
        return header + pose.record();
    }
};
class StageOrthographic {
  public:
    static StageOrthographic load(const Rom &, const StagePlacements &,
                                  const std::function<quint32()> &random = {});
    static StageOrthographic fromRecords(const Rom &, const QVector<QByteArray> &);
    // Called once by the world drawing prepass; repainting must not advance it.
    void advance();
    void resetPose(int index, quint32 selector);
    const QVector<StageFlatProp> &props() const {
        return props_;
    }
    // 0: after background; 1: before foreground; 2: after foreground.
    static int drawPass(const QByteArray &record, float backgroundZ, float foregroundZ);
    static QByteArray modelMatrix(const QByteArray &record, const QByteArray &parent);

  private:
    QVector<StageFlatProp> props_;
};
class StageFlatRenderer {
  public:
    explicit StageFlatRenderer(Rom rom) : rom_(std::move(rom)), frames_(64 * 1024) {}
    void setArtwork(const ActorArtworkEdits &edits) {
        if (artwork_ != edits) {
            artwork_ = edits;
            frames_.clear();
        }
    }
    QVector<ActorRasterLayer> project(const StageOrthographic &, int pass, const SceneCamera &,
                                      const QByteArray &parent, int foregroundSelector,
                                      const std::array<qint16, 3> &globalRgb,
                                      std::optional<RenderColor> transition = {},
                                      bool warmTint = false, RenderColor environment = {},
                                      float backgroundZ = -5, float foregroundZ = -905,
                                      bool sepia = false);

  private:
    Rom rom_;
    QCache<QString, QVector<ActorFrameLayer>> frames_;
    ActorArtworkEdits artwork_;
};
} // namespace lordly
