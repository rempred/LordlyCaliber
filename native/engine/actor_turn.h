#pragma once
#include "engine/rom.h"
#include <optional>
namespace lordly {
// ROM 29C624..29C950: ten-byte slot-owned facing transition.
class ActorTurnJob {
  public:
    ActorTurnJob() = default;
    explicit ActorTurnJob(QByteArray record);
    bool present() const {
        return !record_.isEmpty();
    }
    const QByteArray &record() const {
        return record_;
    }
    void start(const Rom &, int from, int to, int direction, qint32 duration);
    std::optional<int> advance(const Rom &);

  private:
    QByteArray record_;
};
} // namespace lordly
