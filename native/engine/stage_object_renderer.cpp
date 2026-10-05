#include "engine/stage_object_renderer.h"
#include "game/director/actor_render.h"
#include "game/director/actor_world_matrix.h"
#include "game/director/scene_matrix.h"
#include <bit>
#include <memory>
namespace lordly {
QByteArray StageObjectRenderer::modelMatrix(const Rom &rom, const QByteArray &drawing,
                                            const SceneCamera &world, const QByteArray &parent) {
    if (drawing.size() != 192 || parent.size() != 64 || world.bank().size() != 56)
        throw Error("Stage object needs complete drawing and camera records");
    float model[16], base[16], combined[16];
    uint8_t packed[64];
    const bool screen = u8(drawing, 0x14) == 3;
    if (screen)
        lordly_matrix_identity(model);
    else {
        LordlyActorWorldConstants constants{};
        constants.radians = std::bit_cast<float>(u32(rom.bytes(), 0x3ab50));
        for (int i = 0; i < 2; ++i) {
            const int at = 0x2ae390 + i * 16;
            constants.camera_pi[i] = std::bit_cast<double>(quint64(u32(rom.bytes(), at)) << 32 |
                                                           u32(rom.bytes(), at + 4));
            constants.camera_offset[i] = std::bit_cast<double>(
                quint64(u32(rom.bytes(), at + 8)) << 32 | u32(rom.bytes(), at + 12));
        }
        float camera[14];
        for (int i = 0; i < 14; ++i)
            camera[i] = std::bit_cast<float>(u32(world.bank(), i * 4));
        if (!lordly_world_billboard(model, camera, &constants))
            throw Error("Stage object camera basis is invalid");
    }
    for (int i = 0; i < 3; ++i)
        model[12 + i] =
            float(qint16(u16(drawing, 0x1c + i * 2)) + qint16(u16(drawing, 0x22 + i * 2)) +
                  qint16(u16(drawing, 0x50 + i * 2)));
    if (!lordly_matrix_pack(packed, model))
        throw Error("Stage object transform exceeds fixed-point bounds");
    lordly_matrix_unpack(model, packed);
    if (screen)
        lordly_matrix_unpack(base, reinterpret_cast<const uint8_t *>(parent.constData()));
    else {
        lordly_matrix_scale(base, world.modelScale(), world.modelScale(), world.modelScale());
        if (!lordly_matrix_pack(packed, base))
            throw Error("Stage object world scale exceeds fixed-point bounds");
        lordly_matrix_unpack(base, packed);
    }
    lordly_matrix_multiply(combined, model, base);
    if (!lordly_matrix_pack(packed, combined))
        throw Error("Stage object matrix exceeds fixed-point bounds");
    return QByteArray(reinterpret_cast<const char *>(packed), 64);
}
QVector<ActorRasterLayer>
StageObjectRenderer::project(const StageWorldProp &prop, int slot, const SceneCamera &world,
                             const SceneCamera &screen, const QByteArray &parent,
                             const std::array<qint16, 3> &globalRgb, ActorRenderState &state,
                             std::optional<RenderColor> transition, quint32 texturePhase) {
    const auto drawing = prop.record();
    if (u32(drawing, 0x94) & 2 || u8(drawing, 0x37) || u32(prop.owner, 0xa0))
        throw Error("Stage object requires its additional color owner");
    const bool optional = u32(drawing, 0x94) & 4;
    if (optional && !auxiliary_) {
        constexpr int at = 0x3224ba + 0x594280;
        const auto compressed = u32(rom_.bytes(), at), decoded = u32(rom_.bytes(), at + 4);
        if (compressed < 4 || compressed > 65536 || decoded > 65536 ||
            quint64(at) + 4 + compressed > quint64(rom_.bytes().size()))
            throw Error("Stage auxiliary texture exceeds its resource budget");
        qsizetype used = 0;
        const auto bytes = decodeLzss(rom_.bytes().mid(at + 8, compressed - 4), decoded, &used);
        if (used != compressed - 4 || u8(bytes, 2) != 4)
            throw Error("Stage auxiliary texture requires its native intensity format");
        auto texture = decodeN64Image(bytes);
        if ((texture.width & (texture.width - 1)) || (texture.height & (texture.height - 1)))
            throw Error("Stage auxiliary texture requires power-of-two wrapping");
        for (int i = 0; i < texture.width * texture.height; ++i)
            put8(texture.rgba, i * 4 + 3, u8(texture.rgba, i * 4));
        auxiliary_ = std::move(texture);
    }
    const int frame = qint16(u16(drawing, 0x4e)), variant = u32(prop.owner, 0x4c);
    const auto key = QString("%1/%2/%3/%4")
                         .arg(prop.pose.art().descriptorKey())
                         .arg(frame)
                         .arg(variant)
                         .arg(optional);
    auto cached = frames_.object(key);
    if (!cached) {
        auto layers =
            std::make_unique<QVector<ActorFrameLayer>>(prop.pose.art().frame(rom_, frame, variant));
        qsizetype bytes = 0;
        for (auto &layer : *layers) {
            if (optional && layer.pixels.sourceColorFormat != 4) {
                // Clearing converter bit2 selects2043E0: merge alpha and expand
                // each5-bit RGB component by a left shift, without bit replication.
                if (layer.pixels.sourceColorFormat != 3)
                    for (int i = 0; i < layer.pixels.image.rgba.size(); i += 4)
                        for (int c = 0; c < 3; ++c)
                            put8(layer.pixels.image.rgba, i + c,
                                 quint8(((u8(layer.pixels.image.rgba, i + c) * 31 + 127) / 255)
                                        << 3));
                layer.pixels.dualTexture = false;
                layer.pixels.sourceColorFormat = 3;
            }
            bytes += layer.pixels.image.rgba.size() + layer.pixels.colorAlpha.size() + 128;
        }
        const int cost = int(qMax<qsizetype>(1, (bytes + 1023) / 1024));
        if (cost > frames_.maxCost())
            throw Error("Stage object frame exceeds its rendering cache");
        cached = layers.get();
        frames_.insert(key, layers.release(), cost);
    }
    if (!classes_)
        classes_ = ClassData::load(rom_);
    QByteArray records;
    for (const auto &entry : classes_->entries())
        records += entry.record;
    uint8_t size;
    if (!lordly_actor_render_class_size(reinterpret_cast<const uint8_t *>(records.constData()),
                                        int(classes_->entries().size()), 0, variant & 255, &size))
        throw Error("Stage object size refers to an unavailable class");
    // Stage object constant RAM801CFE00, matched with adjacent overlay data.
    const double multiplier =
        size == 2 ? std::bit_cast<double>(quint64(u32(rom_.bytes(), 0x213290)) << 32 |
                                          u32(rom_.bytes(), 0x213294))
                  : 1.;
    const int factor = u8(prop.owner, 0xab) * qint16(u16(drawing, 0x34)) / 255;
    if (!factor)
        return {};
    QByteArray adapter(336, 0);
    put32(adapter, 0xe4, quint32(slot));
    adapter.replace(0xa0, 64, modelMatrix(rom_, drawing, world, parent));
    auto rgb = globalRgb;
    int alpha = factor;
    if (transition) {
        for (int c = 0; c < 3; ++c)
            rgb[c] = qint16(rgb[c] * (*transition)[c] / 255);
        alpha = alpha * (*transition)[3] / 255;
    }
    for (int c = 0; c < 3; ++c)
        put8(adapter, 0x142 + c, quint8(qBound(0, int(rgb[c]), 255)));
    auto material = ActorRenderState::main(state.environment());
    QVector<ActorRasterLayer> result;
    for (int i = 0; i < cached->size(); ++i) {
        if (i >= 16)
            throw Error("Stage object exceeds16 opacity fields");
        if (!u8(drawing, 0x56 + i))
            continue;
        auto layer = (*cached)[i];
        if (u8(drawing, 0x15) & 1)
            layer.metadata.origin.setX(-layer.metadata.origin.x() - layer.metadata.width);
        if (u8(drawing, 0x15) & 3)
            layer.pixels = layer.pixels.oriented(layer.metadata.width, layer.metadata.height,
                                                 u8(drawing, 0x15) & 1, u8(drawing, 0x15) & 2);
        put8(adapter, i, quint8(alpha * u8(drawing, 0x56 + i) / 255));
        QByteArray commands(8, 0);
        put32(commands, 0,
              optional                   ? 0xfc21a3ff
              : layer.pixels.dualTexture ? 0xfc2197ff
                                         : 0xfc1197ff);
        put32(commands, 4, optional ? 0xff0ffe3f : 0xfffffe38);
        material.apply(commands);
        const LayerAuxTexture auxiliary{optional ? &*auxiliary_ : nullptr, texturePhase,
                                        quint8(u8(drawing, 0x15) ^ (layer.metadata.flags & 3))};
        result << ActorRenderer::projectLayer(
            layer, adapter, i, false, u8(drawing, 0x14) == 3 ? screen : world, {255, 255, 255},
            multiplier, 0, 255, &material, LayerBinding::StageObject,
            optional ? &auxiliary : nullptr);
    }
    state = material;
    return result;
}
} // namespace lordly
