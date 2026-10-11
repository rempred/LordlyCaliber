#include "engine/actor_motion.h"
#include <bit>
namespace lordly {
LordlyMotionConstants actorMotionConstants(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Actor motion currently requires US revision 0");
    // Fixed cutscene slab: RAM 0x80239318, not an early-boot mapping.
    const auto read = [&](qsizetype at) {
        return std::bit_cast<double>((quint64(u32(rom.bytes(), at)) << 32) |
                                     u32(rom.bytes(), at + 4));
    };
    return {read(0x2a8b48), read(0x2a8b50), read(0x2a8b58)};
}
} // namespace lordly
