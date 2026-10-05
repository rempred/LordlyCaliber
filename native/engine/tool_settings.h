#pragma once
#include "engine/rom.h"
#include <QJsonArray>
#include <QJsonObject>
namespace lordly {
class ToolSettings {
  public:
    static ToolSettings load(const Rom &);
    static QJsonArray experienceSites();
    bool available() const {
        return unavailable_.isEmpty();
    }
    QString unavailableReason() const {
        return unavailable_;
    }
    int experiencePercent() const {
        return percent_;
    }
    int loadedExperiencePercent() const {
        return original_;
    }
    bool modified() const {
        return percent_ != original_;
    }
    void setExperiencePercent(int);
    QJsonObject json() const {
        return {{"version", 1}, {"experienceScalePercent", percent_}};
    }
    void apply(const QJsonObject &);
    QByteArray assemblyDefinitions(int part) const;
    void validateCompiled(int part, const QByteArray &) const;

  private:
    int percent_ = 100, original_ = 100;
    QString unavailable_;
};
} // namespace lordly
