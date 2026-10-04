#include "engine/actor_main_matrix.h"
#include "game/director/actor_main_matrix.h"
#include <bit>
namespace lordly {
ActorMainMatrix ActorMainMatrix::prepare(const Rom &rom, const QByteArray &actor,
                                         const QByteArray &channel, const SceneCamera &camera) {
    rom.requireSupported();
    if (rom.revision() || actor.size() != 336 || channel.size() != 88)
        throw Error("Main Actor matrices require revision-zero Actor and channel records");
    auto real = [&](int at) { return std::bit_cast<float>(u32(rom.bytes(), at)); };
    auto number = [&](int at) {
        return std::bit_cast<double>(quint64(u32(rom.bytes(), at)) << 32 |
                                     u32(rom.bytes(), at + 4));
    };
    LordlyActorMatrixConstants constants{};
    // Qualified resident and mode-zero/shared overlay data placements, as in
    // the retained shared-Actor projection reference. Never use a boot delta here.
    constants.radians = real(0x3ab50);
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
    LordlyActorMainMatrix result{};
    if (!lordly_actor_main_matrix(&result, reinterpret_cast<const uint8_t *>(actor.constData()),
                                  reinterpret_cast<const uint8_t *>(channel.constData()), bank,
                                  camera.modelScale(), &constants))
        throw Error("Actor main matrix has invalid projection or fixed-point coordinates");
    return {{reinterpret_cast<const char *>(result.matrix), 64},
            {result.position[0], result.position[1]},
            {result.scale[0], result.scale[1]}};
}
} // namespace lordly
