#include "engine/scene_actor_renderer.h"
#include "engine/scene_selection.h"
#include <bit>
namespace lordly {
SceneActorFrame SceneActorRenderer::special(const SceneActors &actors, SceneEffects &effects,
                                            const std::array<quint8, 6> &gradient,
                                            const SceneActorRenderContext &context) {
    SceneActorFrame result{QImage(320, 240, QImage::Format_RGBA8888), {}, context.environment};
    result.image.fill(QColor(0, 0, 0, effects.specialBlackAlpha()));
    if (context.selection)
        context.selection->cover(effects.specialBlackAlpha());
    const auto passes = effects.specialActorPasses(actors.actorRecords());
    if (passes.isEmpty())
        return result;
    auto state = ActorRenderState::main(context.environment);
    const auto &actor = *actors.actor(passes.first().slot);
    const auto record = actor.record();
    state.worldEnvironment(std::bit_cast<float>(u32(record, 0x11c)),
                           std::bit_cast<float>(u32(record, 0x124)), gradient);
    for (const auto &pass : passes) {
        // a2=1 multiplies presentation opacity but does not choose geometry.
        // The native owner overrides opacity; pass.alpha is that resulting factor.
        const auto layers =
            renderer_.project(actor, false, context.camera, context.globalRgb, pass.sepia, 0,
                              pass.alpha, &state, nullptr,
                              actor.pose().art().isBody()
                                  ? std::optional<BodyAppearance>{{actors.bodyEquipment(pass.slot),
                                                                   context.bodyPaletteReplacement}}
                                  : std::nullopt);
        ActorRenderer::paint(result.image, layers);
        if (context.selection)
            context.selection->actors(layers);
        result.layers += layers;
    }
    result.environment = state.environment();
    if (effects.capture().selector() == 4)
        effects.setImageProcessing(false);
    return result;
}
SceneActorFrame SceneActorRenderer::layered(SceneActors &actors, const SceneDrawQueue &queue,
                                            quint8 objectChannel, quint8 channel,
                                            const SceneActorRenderContext &context,
                                            const ObjectRender &objects) {
    const auto selected = queue.modeZeroPasses(actors.actorRecords(), objectChannel, channel);
    QVector<LordlySceneWorldDrawPass> passes;
    for (const auto &pass : selected)
        passes.append(
            {qint16(pass.kind ? 1 : 0), pass.index, qint16(pass.kind == 2 ? 0 : -1), 255});
    return draw(actors, passes, context, objects, {});
}
SceneActorFrame SceneActorRenderer::world(SceneActors &actors, const SceneDrawQueue &queue,
                                          const QVector<QByteArray> &objects,
                                          const std::array<qint16, 2> &shadowAlpha,
                                          const std::array<quint8, 6> &gradient,
                                          const SceneActorRenderContext &context,
                                          const ObjectRender &drawObject) {
    return draw(actors, queue.modeTwoPasses(actors.actorRecords(), objects, shadowAlpha), context,
                drawObject, gradient);
}
SceneActorFrame SceneActorRenderer::draw(SceneActors &actors,
                                         const QVector<LordlySceneWorldDrawPass> &passes,
                                         const SceneActorRenderContext &context,
                                         const ObjectRender &objects,
                                         const std::optional<std::array<quint8, 6>> &gradient) {
    auto next = actors;
    auto images = context.effects ? context.effects->images() : SceneImageCache{};
    SceneActorFrame result{QImage(320, 240, QImage::Format_RGBA8888), {}, context.environment};
    if (result.image.isNull())
        throw Error("Unable to allocate the scene Actor frame");
    result.image.fill(Qt::transparent);
    auto state = ActorRenderState::shadow(context.environment);
    for (const auto &pass : passes) {
        if (pass.kind != 1) {
            if (!objects)
                throw Error("Scene composition requires the queued object's render owner");
            objects(pass.kind, pass.index, result.image, state);
            continue;
        }
        const bool shadow = pass.shadow >= 0;
        if (!shadow) {
            // Both drivers reset the main preset per Actor, retain environment,
            // then run tint/material. World draws replace environment afterward.
            state = ActorRenderState::main(state.environment());
            state.apply(next.renderMaterials(pass.index));
            if (gradient) {
                const auto &record = next.actor(pass.index)->record();
                state.worldEnvironment(std::bit_cast<float>(u32(record, 0x11c)),
                                       std::bit_cast<float>(u32(record, 0x124)), *gradient);
            }
        }
        const auto layers = renderer_.project(
            *next.actor(pass.index), shadow, context.camera, context.globalRgb,
            context.effects && !next.actor(pass.index)->pose().art().isBody()
                ? context.effects->processActorImage(pass.index)
                : context.sepia,
            shadow ? pass.shadow : 0, pass.alpha, &state, context.effects ? &images : nullptr,
            next.actor(pass.index)->pose().art().isBody()
                ? std::optional<BodyAppearance>{{next.bodyEquipment(pass.index),
                                                 context.bodyPaletteReplacement}}
                : std::nullopt);
        ActorRenderer::paint(result.image, layers);
        if (context.selection)
            context.selection->actors(layers);
        result.layers += layers;
    }
    result.environment = state.environment();
    actors = std::move(next);
    if (context.effects)
        context.effects->images() = std::move(images);
    return result;
}
} // namespace lordly
