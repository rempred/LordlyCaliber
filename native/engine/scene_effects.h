#pragma once
#include "engine/rom.h"
#include <QVector>
#include <array>
namespace lordly {
// Scene-owned color transitions and the independent special-actor black pass.
class SceneEffects {
  public:
    void supplyColor(int slot, QByteArray record);
    const QByteArray &colorRecord(int slot) const;
    void setSharedColor(std::array<quint8, 5> rgbaAndMask) {
        shared_ = rgbaAndMask;
    }
    const std::array<quint8, 5> &sharedColor() const {
        return shared_;
    }
    void startColor(const QVector<quint32> &operands);
    void advanceColors();
    void supplySpecialBlack(QByteArray record);
    const QByteArray &specialBlackRecord() const {
        return special_;
    }
    void advanceSpecialBlack();
    quint8 specialBlackAlpha() const;
    static bool visibleAboveSpecialBlack(qint32 source);
    bool service(quint32 opcode, const QVector<quint32> &operands);

  private:
    static void checkSlot(int slot);
    std::array<QByteArray, 20> colors_;
    std::array<quint8, 5> shared_{255, 255, 255, 255, 0};
    QByteArray special_;
};
} // namespace lordly
