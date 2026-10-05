#include "engine/actor_renderer.h"
#include "game/director/actor_render.h"
#include "game/director/scene_matrix.h"
#include <QPainter>
#include <QPainterPath>
#include <bit>
#include <memory>
namespace lordly {
ActorRasterLayer ActorRenderer::projectLayer(const ActorFrameLayer &frame, const QByteArray &actor,
                                             int layer, bool shadow, const SceneCamera &camera,
                                             const std::array<qint16, 3> &globalRgb,
                                             double bodyMultiplier, int shadowIndex, quint8 alpha,
                                             ActorRenderState *state, LayerBinding binding,
                                             const LayerAuxTexture *auxiliary) {
    if (actor.size() != 336)
        throw Error("Actor rendering requires a complete record");
    if (shadowIndex < 0 || shadowIndex > 1)
        throw Error("Actor shadow matrix index is outside 0..1");
    ActorRasterLayer result;
    result.slot = qint32(u32(actor, 0xe4));
    result.layer = layer;
    result.shadow = shadow;
    result.shadowIndex = shadowIndex;
    if (!lordly_actor_primitive(result.primitive.data(),
                                reinterpret_cast<const uint8_t *>(actor.constData()), layer, alpha,
                                shadow, globalRgb.data()))
        throw Error("Actor layer is outside its 16 native opacity bytes");
    const auto &m = frame.metadata;
    const auto &r = frame.pixels.image;
    if (m.width < 1 || m.height < 1 || r.width != m.width || r.height != m.height ||
        qint64(r.width) * r.height * 4 != r.rgba.size())
        throw Error("Actor drawing layer has inconsistent pixel dimensions");
    float scale[16];
    uint8_t packed[64];
    lordly_matrix_scale(scale, float(double(float(m.scaleX) / 1024.f) * bodyMultiplier),
                        float(double(float(m.scaleY) / 1024.f) * bodyMultiplier), 1.f);
    if (!lordly_matrix_pack(packed, scale))
        throw Error("Actor layer scale exceeds the fixed-point range");
    lordly_matrix_unpack(scale, packed);
    // The original vertices narrow to signed16 and use inclusive right/bottom
    // pixel coordinates. Already oriented decoded pixels retain texture flips.
    const double left = qint16(m.origin.x()), right = qint16(m.origin.x() + m.width - 1),
                 top = qint16(-m.origin.y()), bottom = qint16(-m.origin.y() - m.height + 1);
    result.plane = camera.projectPackedPlane(
        actor.mid(shadow ? 0x20 + shadowIndex * 64 : 0xa0, 64),
        {left * scale[0], top * scale[5], 0}, {(right - left) * scale[0] / m.width, 0, 0},
        {0, (bottom - top) * scale[5] / m.height, 0}, QSizeF(m.width, m.height));
    result.image = QImage(m.width, m.height, QImage::Format_RGBA8888);
    if (result.image.isNull())
        throw Error("Unable to allocate an Actor layer image");
    if (frame.pixels.sourceColorFormat == 3 && !auxiliary)
        throw Error("Actor RGBA32 source expansion requires its native layout conversion");
    if (auxiliary && (binding != LayerBinding::StageObject || !auxiliary->image ||
                      auxiliary->image->width <= 0 || auxiliary->image->height <= 0 ||
                      auxiliary->image->rgba.size() !=
                          qint64(auxiliary->image->width) * auxiliary->image->height * 4))
        throw Error("Invalid Stage auxiliary texture");
    if (frame.pixels.dualTexture && frame.pixels.colorAlpha.size() != m.width * m.height)
        throw Error("Actor color texture has no matching alpha lane");
    auto next = state    ? *state
                : shadow ? ActorRenderState::shadow({0, 0, 0, 0})
                         : ActorRenderState::main({0, 0, 0, 0});
    if (binding == LayerBinding::Actor)
        next.bindTexture(frame.pixels.dualTexture);
    const auto combiner = next.combiner();
    CombineInputs inputs;
    inputs.primitive = result.primitive;
    inputs.environment = next.environment();
    inputs.shade = {255, 255, 255, 255};
    for (int y = 0; y < m.height; ++y) {
        auto destination = result.image.scanLine(y);
        const auto source = reinterpret_cast<const quint8 *>(r.rgba.constData()) + y * m.width * 4;
        for (int x = 0; x < m.width; ++x) {
            for (int c = 0; c < 4; ++c)
                inputs.texel0[c] = inputs.texel1[c] = source[x * 4 + c];
            if (frame.pixels.dualTexture) {
                if (binding != LayerBinding::StageFlat) {
                    inputs.texel0.fill(source[x * 4 + 3]);
                    inputs.texel1[3] = u8(frame.pixels.colorAlpha, y * m.width + x);
                } else {
                    inputs.texel1.fill(source[x * 4 + 3]);
                    inputs.texel0[3] = u8(frame.pixels.colorAlpha, y * m.width + x);
                }
            }
            if (auxiliary) {
                const auto &texture = *auxiliary->image;
                // Point sampling approximates the RDP's quarter-texel filtering.
                const auto phase = auxiliary->quarterTexelPhase / 4;
                const int tx =
                    int((quint32(auxiliary->orientation & 1 ? m.width - 1 - x : x) + phase) %
                        quint32(texture.width));
                const int ty =
                    int((quint32(auxiliary->orientation & 2 ? m.height - 1 - y : y) + phase) %
                        quint32(texture.height));
                for (int c = 0; c < 4; ++c)
                    inputs.texel0[c] = u8(texture.rgba, (ty * texture.width + tx) * 4 + c);
            }
            const auto color = combiner.evaluate(inputs);
            for (int c = 0; c < 4; ++c)
                destination[x * 4 + c] = color[c];
        }
    }
    if (state)
        *state = next;
    return result;
}
QVector<ActorRasterLayer> ActorRenderer::project(const ActorInstance &actor, bool shadow,
                                                 const SceneCamera &camera,
                                                 const std::array<qint16, 3> &globalRgb, bool sepia,
                                                 int shadowIndex, quint8 alpha,
                                                 ActorRenderState *state, SceneImageCache *images,
                                                 std::optional<BodyAppearance> appearance) {
    return projectRecord(actor.pose().art(), actor.record(), shadow, camera, globalRgb, sepia,
                         shadowIndex, alpha, state, images, appearance);
}
QVector<ActorRasterLayer>
ActorRenderer::projectRecord(const ActorArt &art, const QByteArray &record, bool shadow,
                             const SceneCamera &camera, const std::array<qint16, 3> &globalRgb,
                             bool sepia, int shadowIndex, quint8 alpha, ActorRenderState *state,
                             SceneImageCache *images, std::optional<BodyAppearance> appearance) {
    if (record.size() != 336)
        throw Error("Actor rendering requires a complete record");
    const auto token = qint32(u32(record, 0xf8));
    const auto variant = art.isBody() ? art.bodyVariant() : u8(record, 0x146);
    if (art.isBody() && !appearance)
        appearance = art.defaultAppearance(rom_);
    QString key = QString("%1/%2/%3/%4/%5")
                      .arg(art.descriptorKey())
                      .arg(art.bodyHandle())
                      .arg(token)
                      .arg(variant)
                      .arg(sepia);
    if (appearance)
        key += QString("/body/%1/%2/%3")
                   .arg(appearance->equipment)
                   .arg(appearance->paletteReplacement ? int(*appearance->paletteReplacement) : -1)
                   .arg(art.sourceClass());
    QVector<quint8> conversions;
    if (images && !art.isBody()) {
        for (const auto &layer : art.layers(token)) {
            const auto count =
                images->touch(qint32(u32(record, 0xe8)), u8(record, 0x146), layer.art, sepia);
            conversions << count;
            key += QString("/%1").arg(count);
        }
    }
    auto cached = frames_.object(key);
    if (!cached) {
        auto frame = std::make_unique<QVector<ActorFrameLayer>>(
            art.isBody() ? art.bodyFrame(rom_, token, *appearance, sepia,
                                         artwork_.value(art.descriptorKey()))
                         : art.frame(rom_, token, variant, sepia, conversions,
                                     artwork_.value(art.descriptorKey())));
        qsizetype bytes = 0;
        for (const auto &layer : *frame)
            bytes += layer.pixels.image.rgba.size() + layer.pixels.colorAlpha.size() + 128;
        const int cost = int(qMax<qsizetype>(1, (bytes + 1023) / 1024));
        if (cost > frames_.maxCost())
            throw Error("Actor frame exceeds the rendering cache budget");
        cached = frame.get();
        frames_.insert(key, frame.release(), cost);
    }
    double multiplier = 1.;
    if (art.isBody()) {
        const auto &classes = art.bodyAnimations()->classes();
        QByteArray records;
        for (const auto &entry : classes.entries())
            records += entry.record;
        uint8_t size = 0;
        if (!lordly_actor_render_class_size(reinterpret_cast<const uint8_t *>(records.constData()),
                                            int(classes.entries().size()), art.sourceClass(),
                                            art.ownerClass(), &size))
            throw Error("Actor body refers to an unavailable class");
        if (size == 2)
            multiplier = std::bit_cast<double>(quint64(u32(rom_.bytes(), 0x2a8b30)) << 32 |
                                               u32(rom_.bytes(), 0x2a8b34));
    }
    auto next = state    ? *state
                : shadow ? ActorRenderState::shadow({0, 0, 0, 0})
                         : ActorRenderState::main({0, 0, 0, 0});
    QVector<ActorRasterLayer> result;
    for (int i = 0; i < cached->size(); ++i)
        result.append(projectLayer((*cached)[i], record, i, shadow, camera, globalRgb, multiplier,
                                   shadowIndex, alpha, &next));
    if (state)
        *state = next;
    return result;
}
void ActorRenderer::paint(QImage &destination, const QVector<ActorRasterLayer> &layers) {
    if (destination.size() != QSize(320, 240))
        throw Error("Actor composition requires a 320 by 240 target");
    QPainter painter(&destination);
    for (const auto &layer : layers) {
        const auto &m = layer.plane.mapping;
        const QTransform transform(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
        if (layer.plane.clip.size() < 3 || !transform.isInvertible())
            continue;
        painter.save();
        QPainterPath clip;
        clip.addPolygon(QPolygonF(layer.plane.clip));
        painter.setClipPath(clip);
        painter.setWorldTransform(transform);
        painter.drawImage(QPointF{}, layer.image);
        painter.restore();
    }
}
} // namespace lordly
