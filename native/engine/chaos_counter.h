#pragma once
#include "engine/rom.h"
#include <QJsonArray>
namespace lordly {
class ChaosCounter {
  public:
    enum class Installation { Retail, Legacy, Native, Unknown };
    static ChaosCounter load(const Rom &);
    static QJsonArray sites();
    static void validateCompiled(const QJsonObject &, const QByteArray &, bool enabled);
    bool available() const {
        return installation_ != Installation::Unknown;
    }
    QString unavailableReason() const {
        return unavailable_;
    }
    Installation installation() const {
        return installation_;
    }
    bool enabled() const {
        return enabled_;
    }
    bool loadedEnabled() const {
        return original_;
    }
    bool modified() const {
        return enabled_ != original_;
    }
    void setEnabled(bool value);

  private:
    Installation installation_ = Installation::Unknown;
    bool enabled_ = false, original_ = false;
    QString unavailable_;
};
} // namespace lordly
