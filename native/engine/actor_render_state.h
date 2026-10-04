#pragma once
#include "engine/color_combiner.h"
namespace lordly {
// Small, explicit display-list state used by the Actor callback. It models the
// color combiner; Qt still approximates the native coverage/blender and sampling.
class ActorRenderState {
  public:
    static ActorRenderState main(RenderColor environment);
    static ActorRenderState shadow(RenderColor environment);
    void apply(const QByteArray &commands);
    void bindTexture(bool dual);
    void worldEnvironment(float x, float z, const std::array<quint8, 6> &gradient);
    ColorCombiner combiner() const {
        return ColorCombiner(high_, low_, twoCycles_);
    }
    RenderColor environment() const {
        return environment_;
    }

  private:
    quint32 high_ = 0, low_ = 0;
    bool twoCycles_ = false;
    RenderColor environment_{};
};
} // namespace lordly
