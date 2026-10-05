#include "engine/scene_actors.h"
#include "engine/scene_transforms.h"
#include <bit>
#include <cmath>
namespace lordly {
void SceneActors::prepareModeZeroMatrices(const SceneCamera &camera,
                                          const SceneTransforms &channels) {
    if (mode_ != 0)
        throw Error("This Actor matrix preparation belongs to mode zero");
    auto next = actors_;
    for (auto &actor : next) {
        if (!actor)
            continue;
        const auto record = actor->record();
        const auto channel = channels.channel(u8(record, 0x13e));
        actor->setDrawing(ActorMatrices::prepare(rom_, record, channel, camera));
    }
    actors_ = std::move(next);
}
void SceneActors::prepareModeTwoMatrices(const SceneCamera &camera,
                                         const LordlyActorLighting &lighting) {
    if (mode_ != 2)
        throw Error("World Actor matrix preparation belongs to mode two");
    auto next = actors_;
    for (int slot = 0; slot < 28; ++slot) {
        auto &actor = next[slot];
        if (!actor)
            continue;
        auto record = actor->record();
        // ROM2AA3B0 prepares linked secondary bodies before their matrices and
        // before the depth queue. Search slot order for the same row's primary.
        if (classification(record) == 2) {
            const auto identity = u8(record, 0x147);
            for (const auto &primary : next) {
                if (!primary)
                    continue;
                const auto candidate = primary->record();
                if (u8(candidate, 0x147) != identity || classification(candidate) != 1)
                    continue;
                auto position = primary->pose().actor().position;
                const float offset = qint16(u16(record, 0x13a)) == 1 ? 8.f : -8.f;
                position.x += offset;
                position.z -= offset;
                if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
                    !std::isfinite(position.z))
                    throw Error("Linked Actor placement produced a nonfinite coordinate");
                actor->pose().actor().position = position;
                record = actor->record();
                break;
            }
        }
        float ground = 0;
        if (u8(record, 0x145) & 1) {
            if (!height_)
                throw Error("Actor shadow requires scene terrain height");
            auto coordinate = [&](int offset) {
                const auto value = std::bit_cast<float>(u32(record, offset));
                if (!std::isfinite(value) || value < -2147483648.f || value >= 2147483648.f)
                    throw Error("Actor terrain coordinate exceeds the native integer range");
                return qint16(qint32(value));
            };
            ground = height_(2, coordinate(0x11c), coordinate(0x124));
        }
        const auto identity = u32(record, 0xe4);
        if (identity >= depths_.size())
            throw Error("Actor drawing depth identity is outside the scene table");
        const qint16 depth = depths_[identity].isEmpty() ? 0 : qint16(u16(depths_[identity], 0));
        actor->setDrawing(
            ActorWorldMatrices::prepare(rom_, record, camera, depth, ground, lighting));
    }
    actors_ = std::move(next);
}
std::array<QByteArray, 28> SceneActors::actorRecords() const {
    std::array<QByteArray, 28> records;
    for (int i = 0; i < 28; ++i)
        if (actors_[i])
            records[i] = actors_[i]->record();
    return records;
}
} // namespace lordly
