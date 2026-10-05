#pragma once
#include "engine/animation_override_image.h"
#include "engine/animation_routes.h"
#include "engine/class_data.h"
#include <QJsonArray>
#include <optional>
namespace lordly {
class AnimationOverrides {
  public:
    static AnimationOverrides load(const Rom &, const ClassData &, const AnimationRoutes &);
    static QVector<int> selectors(const Rom &, const AnimationRoutes &, int classId,
                                  std::optional<int> bodyFlags = {});
    bool available() const {
        return reason_.isEmpty();
    }
    QString unavailableReason() const {
        return reason_;
    }
    int loadedVersion() const {
        return image_.version();
    }
    bool modified() const {
        return entries_ != original_;
    }
    bool needsRebuild(const ClassData &) const;
    const QJsonArray &entries() const {
        return entries_;
    }
    QJsonObject json() const;
    void apply(const QJsonObject &, const Rom &, const ClassData &, const AnimationRoutes &);
    void importLegacy(const QJsonObject &, int projectVersion, const Rom &, const ClassData &,
                      const AnimationRoutes &);
    void setEntries(const QJsonArray &, const Rom &, const ClassData &, const AnimationRoutes &);
    QByteArray table(const ClassData &) const;
    QByteArray moduleData(const ClassData &) const;
    const AnimationOverrideImage &image() const {
        return image_;
    }

  private:
    AnimationOverrideImage image_;
    QJsonArray original_, entries_;
    QByteArray originalTable_;
    QString reason_;
};
} // namespace lordly
