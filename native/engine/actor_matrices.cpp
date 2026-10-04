#include "engine/actor_matrices.h"
#include "game/director/actor_matrices.h"
#include <bit>
namespace lordly {
ActorMatrices ActorMatrices::prepare(const Rom &rom, const QByteArray &actor,
                                     const QByteArray &channel, const SceneCamera &camera) {
    rom.requireSupported();
    if (rom.revision() || actor.size() != 336 || channel.size() != 88)
        throw Error("Actor matrices require revision-zero Actor and channel records");
    auto real = [&](int at) { return std::bit_cast<float>(u32(rom.bytes(), at)); };
    auto number = [&](int at) {
        return std::bit_cast<double>(quint64(u32(rom.bytes(), at)) << 32 |
                                     u32(rom.bytes(), at + 4));
    };
    LordlyActorMatrixConstants constants{};
    // Qualified resident and mode-zero/shared overlay data placements, as in
    // the retained shared-Actor projection reference. Never use a boot delta here.
    constants.radians = real(0x3ab50);
    constants.axis_radians = real(0x3bee0);
    constants.shadow_pi = number(0x2b8a38);
    constants.half_turn = number(0x2b8a58);
    for (int i = 0; i < 10; ++i)
        constants.facing[i] = qint16(u16(rom.bytes(), 0x2b8a40 + i * 2));
    constants.inverse_radians = real(0x39c80);
    constants.pi = number(0x2b8a28);
    constants.quarter_turn = number(0x2b8a30);
    for (int i = 0; i < 4; ++i) {
        constants.projection[0][i] = number(0x2a89e0 + i * 8);
        constants.projection[1][i] = number(0x2a8b60 + i * 8);
    }
    for (int i = 0; i < 3; ++i) {
        constants.origin[i] = real(0x2b8a00 + i * 4);
        constants.probe_x[i] = real(0x2b8a0c + i * 4);
        constants.probe_y[i] = real(0x2b8a18 + i * 4);
    }
    float bank[14];
    for (int i = 0; i < 14; ++i)
        bank[i] = std::bit_cast<float>(u32(camera.bank(), i * 4));
    LordlyActorMatrices result{};
    if (!lordly_actor_matrices(&result, reinterpret_cast<const uint8_t *>(actor.constData()),
                               reinterpret_cast<const uint8_t *>(channel.constData()), bank,
                               camera.modelScale(), &constants))
        throw Error("Actor matrix has invalid projection or fixed-point coordinates");
    return {{reinterpret_cast<const char *>(result.matrix), 64},
            {reinterpret_cast<const char *>(result.shadow), 64},
            {result.position[0], result.position[1]},
            {result.scale[0], result.scale[1]}};
}
} // namespace lordly
