#pragma once
#include "engine/rom.h"
#include <QRect>
#include <QVector>
#include <array>
namespace lordly {
// Cutscene scissor owner. The title-card variant has a separate textured renderer.
class SceneCrop {
  public:
    void start(const QVector<quint32> &);
    void draw();
    bool active() const;
    const QByteArray &record() const {
        return record_;
    }
    const std::array<quint32, 2> &scissor() const {
        return scissor_;
    }
    QRect clip() const;

  private:
    QByteArray record_;
    std::array<quint32, 2> scissor_{0xed000060U, 0x004fc35cU};
};
} // namespace lordly
