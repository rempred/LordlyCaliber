#include "engine/sprite_renderer.h"
#include "game/director/scene_matrix.h"
#include <bit>
namespace lordly {
SpriteMatrices SpriteMatrices::prepare(const Rom &rom, const SceneSpriteDraw &draw,
                                       const QByteArray &parent) {
    rom.requireSupported();
    if (parent.size() != 64 || rom.revision())
        throw Error(
            "Sprite projection requires a packed parent matrix and revision-zero constants");
    auto pack = [](const float matrix[16]) {
        QByteArray result(64, 0);
        if (!lordly_matrix_pack(reinterpret_cast<uint8_t *>(result.data()), matrix))
            throw Error("Sprite transform exceeds the native fixed-point range");
        return result;
    };
    float local[16], combined[16], part[16];
    SpriteMatrices result;
    lordly_matrix_translate(local, draw.x, draw.y, 0);
    result.translation = pack(local);
    lordly_matrix_rotation(local, 0, 0, draw.rotation,
                           std::bit_cast<float>(u32(rom.bytes(), 0x3bee0)));
    result.rotation = pack(local);
    lordly_matrix_scale(local, draw.scale, draw.scale, draw.scale);
    result.scale = pack(local);
    lordly_matrix_unpack(combined, reinterpret_cast<const uint8_t *>(parent.constData()));
    for (const auto &matrix : {result.translation, result.rotation, result.scale}) {
        lordly_matrix_unpack(part, reinterpret_cast<const uint8_t *>(matrix.constData()));
        lordly_matrix_multiply(combined, part, combined);
    }
    result.combined = pack(combined);
    return result;
}
QVector<ActorRasterLayer> SpriteRenderer::project(const SceneSprites &sprites, qint32 pass,
                                                  quint8 special, const SceneCamera &camera,
                                                  const QByteArray &parent,
                                                  const std::array<qint16, 3> &globalRgb,
                                                  const QByteArray &colorTransition, bool sepia) {
    QVector<ActorRasterLayer> result;
    for (const auto &draw : sprites.draws(pass, special)) {
        const auto &sprite = *sprites.sprite(draw.slot);
        const auto matrices = SpriteMatrices::prepare(rom_, draw, parent);
        const auto &pose = sprite.sprite();
        // Both callbacks use the same layer vertices and texture lanes. A local
        // draw record adapts those shared rules without inventing a game Actor.
        QByteArray record(336, 0);
        for (int i = 0; i < 16; ++i)
            put8(record, i, pose.color[i]);
        put32(record, 0xe4, quint32(draw.slot));
        put32(record, 0xf8, quint32(pose.frame));
        put8(record, 0x146, u8(sprite.record(), 0x4c));
        const auto primitive =
            sprites.primitiveColor(draw.slot, 0, 255, globalRgb, colorTransition);
        for (int i = 0; i < 3; ++i)
            put8(record, 0x142 + i, primitive[i]);
        record.replace(0xa0, 64, matrices.combined);
        // ROM29AA18 resets this preset and environment for every eligible sprite.
        auto state = ActorRenderState::main({0, 0, 0, 1});
        result += renderer_.projectRecord(sprite.art(), record, false, camera, {255, 255, 255},
                                          sepia, 0, 255, &state);
    }
    return result;
}
} // namespace lordly
