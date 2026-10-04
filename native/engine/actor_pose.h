#pragma once
#include "engine/actor_art.h"
#include "game/director/actor_pose.h"
namespace lordly {
struct PoseAudioRequest {
    int context = 0;
    quint16 value = 0;
};
class ActorPosePlayer {
  public:
    ActorPosePlayer(const Rom &rom, ActorArt art, int state, int variant = 0);
    int advance(int budget = 256);
    const LordlyActorPose &actor() const {
        return actor_;
    }
    LordlyActorPose &actor() {
        return actor_;
    }
    const ActorArt &art() const {
        return art_;
    }
    const QVector<PoseAudioRequest> &audioRequests() const {
        return audio_;
    }
    QString error() const {
        return error_;
    }
    int result() const {
        return invocation_.result;
    }
    bool suspended() const {
        return invocation_.active != 0;
    }
    int query() const;
    void setLinkedObjectKnownAbsent(bool absent) {
        linkedObjectAbsent_ = absent;
    }

  private:
    static int lookup(void *, const LordlyActorPose *, LordlyPoseRecord *);
    static int shared(void *, const LordlyActorPose *, const LordlyPoseRecord *);
    Rom rom_;
    ActorArt art_;
    LordlyActorPose actor_{};
    LordlyPoseInvocation invocation_{};
    QVector<PoseAudioRequest> audio_;
    QString error_;
    bool linkedObjectAbsent_ = false;
};
} // namespace lordly
