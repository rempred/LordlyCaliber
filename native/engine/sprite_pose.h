#pragma once
#include "engine/actor_pose.h"
#include "game/director/sprite_pose.h"
namespace lordly {
// Lossless 80-byte scene-sprite record and its independent pose continuation.
class SpritePosePlayer {
  public:
    SpritePosePlayer(const Rom &, ActorArt, int state, int variant = 0);
    SpritePosePlayer(const Rom &, const QByteArray &record);
    SpritePosePlayer(const Rom &, ActorArt, const QByteArray &record);
    int advance(int budget = 256);
    int query() const;
    QByteArray record() const;
    LordlySpritePose &sprite() {
        return sprite_;
    }
    const LordlySpritePose &sprite() const {
        return sprite_;
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
    void setRotation(qint16 rotation);

  private:
    static int lookup(void *, const LordlySpritePose *, LordlyPoseRecord *);
    static int sound(void *, const LordlySpritePose *, const LordlyPoseRecord *);
    Rom rom_;
    ActorArt art_;
    QByteArray record_;
    LordlySpritePose sprite_{};
    LordlyPoseInvocation invocation_{};
    QVector<PoseAudioRequest> audio_;
    QString error_;
};
} // namespace lordly
