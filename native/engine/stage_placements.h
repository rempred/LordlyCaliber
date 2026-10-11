#pragma once
#include "engine/rom.h"
#include <QVector>
namespace lordly {
enum class StagePlacementKind { Orthographic, Perspective };
struct StagePlacement {
    StagePlacementKind kind;
    QByteArray source;
    bool randomized() const {
        return qint16(u16(source, 0)) == -2;
    }
    // Original resolver's sparse output. Unwritten fields retain the caller's bytes.
    QByteArray resolve(QByteArray destination = {}) const;
};
class StagePlacements {
  public:
    static StagePlacements load(const Rom &, int foregroundSelector);
    const QVector<StagePlacement> &orthographic() const {
        return orthographic_;
    }
    const QVector<StagePlacement> &perspective() const {
        return perspective_;
    }

  private:
    QVector<StagePlacement> orthographic_, perspective_;
};
} // namespace lordly
