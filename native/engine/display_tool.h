#pragma once
#include "engine/tool_artwork.h"
#include <QJsonObject>
namespace lordly {
class DisplayTool {
  public:
    enum class Installation { Retail, Legacy, Native, Unknown };
    static DisplayTool load(const Rom &, ToolLabel);
    static QJsonObject definition(ToolLabel);
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
