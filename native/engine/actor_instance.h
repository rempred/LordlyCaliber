#pragma once
#include "engine/actor_matrices.h"
#include "engine/actor_pose.h"
#include "engine/actor_world_matrices.h"
#include "game/director/actor_record.h"
#include <functional>
namespace lordly {
// A lossless native record with explicit host ownership and current-ROM pose binding.
class ActorInstance {
  public:
    using Height = std::function<float(int mode, float x, float z)>;
    ActorInstance(const Rom &rom, const QByteArray &record, BodyAnimationData::Pointer data = {});
    static ActorInstance create(const Rom &rom, const LordlyActorSelection &selection, int mode,
                                const Height &height = {}, BodyAnimationData::Pointer data = {});
    void select(const LordlyActorSelection &selection);
    QByteArray record() const;
    void setSlot(int slot);
    void setOpacity(quint8 value);
    void setTint(quint8 red, quint8 green, quint8 blue);
    void setChannel(quint8 channel);
    void setDrawing(const ActorMatrices &);
    void setDrawing(const ActorWorldMatrices &);
    void setUniformScale(float value);
    void addHeight(float step);
    void setSecondaryHeight(float value, bool addBase);
    void resetHeight(qint32 value);
    ActorPosePlayer &pose() {
        return pose_;
    }
    const ActorPosePlayer &pose() const {
        return pose_;
    }

  private:
    Rom rom_;
    BodyAnimationData::Pointer bodyData_;
    QByteArray record_;
    ActorPosePlayer pose_;
};
} // namespace lordly
