#include "engine/actor_world_matrices.h"
#include <bit>
namespace lordly {
ActorWorldMatrices ActorWorldMatrices::prepare(const Rom &rom, const QByteArray &actor,
                                               const SceneCamera &camera, qint16 depth,
                                               float terrainHeight,
                                               const LordlyActorLighting &lighting) {
    rom.requireSupported();
    if (rom.revision() || actor.size() != 336)
        throw Error("World Actor matrices require a revision-zero complete Actor record");
    auto number = [&](int at) {
        return std::bit_cast<double>(quint64(u32(rom.bytes(), at)) << 32 |
                                     u32(rom.bytes(), at + 4));
    };
    LordlyActorWorldConstants c{};
    c.radians = std::bit_cast<float>(u32(rom.bytes(), 0x3ab50));
    // The mode-two data block is separate from mode-zero. Complete64 bytes
    // ROM2AE370 == RAM8023EC10 in the retained opening-ceremony state;
    // the complete producer and camera functions also match their live placements.
    c.half_turn = number(0x2ae370);
    c.pi = number(0x2ae378);
    c.slope = number(0x2ae380);
    c.intercept = number(0x2ae388);
    for (int i = 0; i < 2; ++i) {
        c.camera_pi[i] = number(0x2ae390 + i * 16);
        c.camera_offset[i] = number(0x2ae398 + i * 16);
    }
    c.point_half_turn = number(0x2a8a50);
    c.point_pi = number(0x2a8a58);
    c.round_threshold = number(0x3e800);
    float bank[14];
    for (int i = 0; i < 14; ++i)
        bank[i] = std::bit_cast<float>(u32(camera.bank(), i * 4));
    LordlyActorWorldMatrices result{};
    if (!lordly_actor_world_matrices(&result, reinterpret_cast<const uint8_t *>(actor.constData()),
                                     bank, camera.modelScale(), depth, terrainHeight, &lighting,
                                     &c))
        throw Error("World Actor matrix has invalid lighting, coordinates or fixed-point range");
    return {{reinterpret_cast<const char *>(result.matrix), 64},
            {{{reinterpret_cast<const char *>(result.shadow[0]), 64},
              {reinterpret_cast<const char *>(result.shadow[1]), 64}}}};
}
} // namespace lordly
