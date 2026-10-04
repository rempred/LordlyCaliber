#pragma once
#include "engine/actor_pose.h"
#include "game/director/actor_record.h"
#include <functional>
namespace lordly {
// A lossless native record with explicit host ownership and current-ROM pose binding.
class ActorInstance {
  public:
    using Height = std::function<float(int mode, float x, float z)>;
    ActorInstance(const Rom &rom, const QByteArray &record);
    static ActorInstance create(const Rom &rom, const LordlyActorSelection &selection, int mode,
                                const Height &height = {});
    void select(const LordlyActorSelection &selection);
    QByteArray record() const;
    void setSlot(int slot);
    void setOpacity(quint8 value);
    void setTint(quint8 red, quint8 green, quint8 blue);
    void setUniformScale(float value);
    void addHeight(float step);
    ActorPosePlayer &pose() {
        return pose_;
    }
    const ActorPosePlayer &pose() const {
        return pose_;
    }

  private:
    Rom rom_;
    QByteArray record_;
    ActorPosePlayer pose_;
};
} // namespace lordly
