#pragma once
#include "engine/rom.h"
#include <QImage>
#include <array>
namespace lordly {
// Value state so seeking a preview restores the image buffers, countdowns and random sequence.
class SceneChapterCard {
  public:
    SceneChapterCard(const Rom &, int selector, int scenario, int sequenceBefore);
    void startFade();
    void startReveal();
    bool revealing() const;
    void advance();
    QImage render() const;

  private:
    int selector_, count_ = 0;
    std::array<QByteArray, 5> source_, pixels_;
    std::array<qint16, 5> x_{}, y_{};
    std::array<quint16, 5> ticks_{}, alpha_{}, step_{};
    quint8 revealSlot_ = 0;
    quint32 random_ = 1;
    void run(bool start);
    std::array<QImage, 5> mission_;
    QImage companionBase_, companionPattern_, companionOverlay_;
    int scrollA_ = 0, scrollB_ = 160;
};
} // namespace lordly
