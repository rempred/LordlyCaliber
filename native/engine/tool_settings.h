#pragma once
#include "engine/chaos_counter.h"
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
        return experienceModified() || chaos_.modified();
    }
    bool experienceModified() const {
        return percent_ != original_;
    }
    const ChaosCounter &chaosCounter() const {
        return chaos_;
    }
    void setExperiencePercent(int);
    QJsonObject json() const {
        return {{"version", 2},
                {"experienceScalePercent", percent_},
                {"chaosFrameCounter", chaos_.enabled()}};
    }
    void apply(const QJsonObject &);
    QByteArray assemblyDefinitions(int part) const;
    void validateCompiled(int part, const QByteArray &) const;

  private:
    int percent_ = 100, original_ = 100;
    QString unavailable_;
    ChaosCounter chaos_;
};
} // namespace lordly
