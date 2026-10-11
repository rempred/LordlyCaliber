#pragma once
#include "engine/actor_art.h"
#include "game/director/actor_pose.h"
#include <optional>
namespace lordly {
struct PoseAudioRequest {
    int context = 0;
    quint16 value = 0;
};
// Native sound-service boundary. The consumer needs the linked object's
// spatial/equipment state to resolve the final sound; pose execution does not.
struct LinkedPoseAudioContext {
    quint32 objectAddress = 0;
    qint32 source = 0, owner = 0;
};
struct LinkedPoseAudioRequest {
    LinkedPoseAudioContext linked;
    quint16 key = 0;
    quint8 opcode = 0;
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
    const QVector<LinkedPoseAudioRequest> &linkedAudioRequests() const {
        return linkedAudio_;
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
    // Alternate counted-program completion check, distinct from Director 0x0F.
    bool bodyBusy() const;
    void setLinkedObjectKnownAbsent(bool absent) {
        linkedObjectAbsent_ = absent;
        linkedAudioContext_.reset();
    }
    void setLinkedAudioContext(LinkedPoseAudioContext context);

  private:
    static int registered(void *, const LordlyActorPose *);
    static int fixed(void *, const LordlyActorPose *);
    static int lookup(void *, const LordlyActorPose *, LordlyPoseRecord *);
    static int shared(void *, const LordlyActorPose *, const LordlyPoseRecord *);
    Rom rom_;
    ActorArt art_;
    LordlyActorPose actor_{};
    LordlyPoseInvocation invocation_{};
    QVector<PoseAudioRequest> audio_;
    QVector<LinkedPoseAudioRequest> linkedAudio_;
    std::optional<LinkedPoseAudioContext> linkedAudioContext_;
    QString error_;
    bool linkedObjectAbsent_ = false;
};
} // namespace lordly
