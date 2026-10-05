#pragma once
#include "engine/rom.h"
#include <QImage>
#include <QVector>
#include <array>
namespace lordly {
class SceneTitleReveal {
  public:
    void start(const QVector<quint32> &);
    void advance();
    bool active() const;
    const QByteArray &record() const {
        return record_;
    }
    std::array<quint32, 8> rectangles(bool highResolution) const;
    // Full-frame transparent overlay. Desktop rectangle coverage is approximate.
    QImage render(bool highResolution) const;

  private:
    QByteArray record_;
};
} // namespace lordly
