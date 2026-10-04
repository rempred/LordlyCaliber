#include "engine/scene_actors.h"
#include "engine/scene_transforms.h"
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
std::array<QByteArray, 28> SceneActors::actorRecords() const {
    std::array<QByteArray, 28> records;
    for (int i = 0; i < 28; ++i)
        if (actors_[i])
            records[i] = actors_[i]->record();
    return records;
}
} // namespace lordly
