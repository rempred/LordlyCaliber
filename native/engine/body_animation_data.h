#pragma once
#include "engine/animation_overrides.h"
#include <memory>
namespace lordly {
// Immutable project snapshot, shared by playback checkpoints and their Actors.
// Resource pixels still come from the opened ROM or the separate artwork edits.
class BodyAnimationData {
  public:
    using Pointer = std::shared_ptr<const BodyAnimationData>;
    BodyAnimationData(ClassData classes, AnimationRoutes routes,
                      std::optional<AnimationOverrides> overrides = {});
    static Pointer load(const Rom &);
    const ClassData &classes() const {
        return classes_;
    }
    const AnimationRoutes &routes() const {
        return routes_;
    }
    quint16 handle(int source, int owner, int flagA, int flagB) const;
    int selector(int source, int owner, int action, int mode, int bodyFlags) const;

  private:
    ClassData classes_;
    AnimationRoutes routes_;
    QVector<AnimationOverrideImage::Row> overrides_;
    QString overrideError_;
};
} // namespace lordly
