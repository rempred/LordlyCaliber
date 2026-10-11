#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <QMap>
namespace lordly {
class EncounterRate {
  public:
    static constexpr int romOffset = 0x13c060, codeSize = 1084;
    static constexpr quint32 ramAddress = 0x801e7920;
    static EncounterRate load(const Rom &rom);
    static QJsonObject catalog();
    static QMap<QString, quint32> serviceAddresses();
    static double passingFraction(quint32 divisor, quint32 passCount);
    quint32 divisor() const {
        return divisor_;
    }
    quint32 normalPassCount() const {
        return normal_;
    }
    quint32 alternatePassCount() const {
        return alternate_;
    }
    bool modified() const {
        return json() != original_;
    }
    QJsonObject json() const;
    void apply(const QJsonObject &object);
    void importLegacy(const QJsonObject &object);
    QByteArray assemblyDefinitions(bool native = true) const;

  private:
    quint32 divisor_ = 72000, normal_ = 51, alternate_ = 11;
    QJsonObject original_;
};
} // namespace lordly
