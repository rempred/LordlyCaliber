#pragma once
#include "engine/rom.h"
#include "game/director/image_motion.h"
#include <QVector>
#include <array>
#include <optional>
namespace lordly {
// Scripted image motion owns its16-byte job. The image presentation's848-byte
// context and channel-one88-byte transform remain explicit, separately owned data.
class SceneImageMotion {
  public:
    explicit SceneImageMotion(const Rom &);
    void start(const QVector<quint32> &, QByteArray &context, const QByteArray &channel);
    void advance(QByteArray &context, QByteArray &channel);
    void manual(QByteArray &context, QByteArray &channel, LordlyImageInput input) const;
    bool active() const {
        return motion_.has_value();
    }
    QByteArray record() const;
    void supplyRecord(QByteArray);

  private:
    std::optional<LordlyImageMotion> motion_;
    std::array<double, 4> constants_;
};
} // namespace lordly
