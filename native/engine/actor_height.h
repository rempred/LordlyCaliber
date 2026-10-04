#pragma once
#include "engine/rom.h"
#include <optional>
namespace lordly {
// Portable reconstruction of ROM 29A59C..29A7C4. Jobs belong to scene slots.
class ActorHeightJob {
  public:
    ActorHeightJob() = default;
    explicit ActorHeightJob(QByteArray record);
    const QByteArray &record() const {
        return record_;
    }
    bool present() const {
        return !record_.isEmpty();
    }
    void start(const Rom &, qint32 interval, qint32 height, qint32 duration,
               std::optional<float> baseY);
    // Advances/possibly expires the job. A value requests a height application;
    // the owning scene must apply it to an actual actor atomically with this job.
    std::optional<float> advance();

  private:
    QByteArray record_;
};
} // namespace lordly
