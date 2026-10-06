#pragma once
#include "engine/direct_pose.h"
#include "engine/stage_placements.h"
#include <array>
namespace lordly {
struct StageWorldProp {
    QByteArray owner;   // Native248-byte owner; host identities replace pointers.
    QByteArray drawing; // Native192-byte drawable, including its80-byte pose.
    DirectPosePlayer pose;
    QByteArray record() const {
        auto result = drawing;
        result.replace(0x44, 80, pose.record());
        return result;
    }
};
class StagePerspective {
  public:
    static StagePerspective load(const Rom &, const StagePlacements &,
                                 const std::array<quint8, 3> &initialRgb,
                                 const std::function<quint32()> &random = {});
    static StagePerspective fromRecords(const Rom &, const QVector<QByteArray> &owners,
                                        const QVector<QByteArray> &drawings);
    void advance();
    void resetPose(int index, quint32 selector);
    const QVector<StageWorldProp> &props() const {
        return props_;
    }
    QVector<QByteArray> records() const;

  private:
    QVector<StageWorldProp> props_;
};
} // namespace lordly
