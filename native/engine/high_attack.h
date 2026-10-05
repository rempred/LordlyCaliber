#pragma once
#include "engine/rom.h"
#include <QJsonObject>
namespace lordly {
class HighAttack {
  public:
    enum class Installation { Retail, Legacy, Native, Unknown };
    static HighAttack load(const Rom &);
    static QJsonObject definition();
    static QByteArray assembly(const QJsonObject &site, bool enabled);
    static void validateCompiled(const QJsonObject &site, const QByteArray &, bool enabled);
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
    void setEnabled(bool);

  private:
    Installation installation_ = Installation::Unknown;
    bool enabled_ = false, original_ = false;
    QString unavailable_;
};
} // namespace lordly
