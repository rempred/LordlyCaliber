#pragma once
#include "engine/actor_art.h"
#include "game/director/direct_pose.h"
#include <functional>
namespace lordly {
struct DirectPoseSound {
    quint8 opcode;
    quint16 key;
    quint32 linkedObject;
};
class DirectPosePlayer {
  public:
    DirectPosePlayer(const Rom &, QByteArray record);
    DirectPosePlayer(const Rom &, quint16 handle, quint16 selector);
    int advance(int budget = 256, const std::function<quint32()> &random = {});
    const QByteArray &record() const {
        return record_;
    }
    const ActorArt &art() const {
        return art_;
    }
    const QVector<DirectPoseSound> &sounds() const {
        return sounds_;
    }
    QString error() const {
        return error_;
    }

  private:
    static int lookup(void *, const uint8_t *, LordlyPoseRecord *);
    static int sound(void *, const uint8_t *, const LordlyPoseRecord *);
    Rom rom_;
    ActorArt art_;
    QByteArray record_;
    LordlyPoseInvocation invocation_{};
    QVector<DirectPoseSound> sounds_;
    QString error_;
};
} // namespace lordly
