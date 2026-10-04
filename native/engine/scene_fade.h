#pragma once
#include "engine/rom.h"
#include <QVector>
#include <array>
namespace lordly {
// Independent permanent-scheduler effect. Calling start does not advance time.
class SceneFade {
  public:
    SceneFade() = default;
    SceneFade(QByteArray record, bool slotActive);
    bool present() const {
        return !record_.isEmpty();
    }
    bool slotActive() const {
        return active_;
    }
    const QByteArray &record() const {
        return record_;
    }
    int remaining() const;
    std::array<quint8, 4> rgba() const;
    // Returns whether a scheduler slot registration was requested.
    bool start(qint32 duration, qint32 red, qint32 green, qint32 blue, qint32 from, qint32 to);
    void markRelease();
    // Returns the native callback's slot-release request. The scheduler must
    // call cleanup before removing that slot; a timed fade can remain visible.
    bool advance();
    void cleanup();
    bool service(quint32 opcode, const QVector<quint32> &operands, int &value);

  private:
    QByteArray record_;
    bool active_ = false;
};
} // namespace lordly
