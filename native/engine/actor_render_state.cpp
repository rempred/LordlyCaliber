#include "engine/actor_render_state.h"
#include "game/director/actor_render.h"
namespace lordly {
ActorRenderState ActorRenderState::main(RenderColor environment) {
    ActorRenderState result;
    result.high_ = 0xfc62fe03;
    result.low_ = 0x2f0d73fd;
    result.twoCycles_ = true;
    result.environment_ = environment;
    return result;
}
ActorRenderState ActorRenderState::shadow(RenderColor environment) {
    auto result = main(environment);
    result.high_ = 0xfcff97ff;
    result.low_ = 0xff2fffff;
    result.twoCycles_ = false;
    return result;
}
void ActorRenderState::apply(const QByteArray &commands) {
    if (commands.size() % 8 || commands.size() > 4096)
        throw Error("Actor material command list is truncated or oversized");
    auto next = *this;
    for (qsizetype at = 0; at < commands.size(); at += 8) {
        const auto high = u32(commands, at), low = u32(commands, at + 4);
        if (high == 0xe7000000 && low == 0)
            continue;
        if (high >> 24 == 0xfc) {
            next.high_ = high;
            next.low_ = low;
        } else if (high == 0xfb000000) {
            for (int c = 0; c < 4; ++c)
                next.environment_[c] = quint8(low >> (24 - c * 8));
        } else if (high == 0xe3000a01 && (low == 0 || low == 0x00100000)) {
            next.twoCycles_ = low != 0;
        } else if (high == 0xe200001c &&
                   (low == 0x00104240 || low == 0x00504240 || low == 0x0c184240)) {
            // These Actor render modes use the desktop source-over compositor.
        } else {
            throw Error("Unsupported Actor material display-list command");
        }
    }
    // Validate every mux before committing, even for a fully clipped image.
    next.combiner().evaluate({});
    *this = next;
}
void ActorRenderState::bindTexture(bool dual) {
    if (!dual) {
        // ROM29EEF8: a single texture replaces the active combiner, and this
        // change remains in effect for subsequent layers until reset by a caller.
        high_ = 0xfc62fe03;
        low_ = 0x1f0cf3fd;
    }
}
void ActorRenderState::worldEnvironment(float x, float z, const std::array<quint8, 6> &gradient) {
    RenderColor next;
    if (!lordly_actor_world_environment(next.data(), x, z, gradient.data()))
        throw Error("Actor environment requires finite world coordinates");
    environment_ = next;
}
} // namespace lordly
