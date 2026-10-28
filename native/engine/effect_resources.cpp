#include "engine/effect_resources.h"
namespace lordly {
QByteArray retailEffectResource(const Rom &rom, int type, int id) {
    rom.requireSupported();
    if (rom.revision() || type < 3 || type > 8 || id < 1 || id > 65535)
        throw Error("Invalid retail effect resource ID");
    const auto &b = rom.bytes();
    const int descriptor = 0x387c0 + 12 * 40;
    const auto ram = u32(b, descriptor), begin = u32(b, descriptor + 8), end = u32(b, descriptor + 12);
    const auto table = 0x80220210U + type * 4;
    if (table < ram || quint64(table) + 4 > quint64(ram) + end - begin)
        throw Error("Effect resource directory is outside its overlay");
    const auto key = u32(b, begin + table - ram);
    const quint64 directory = 0x594280ULL + key;
    if (directory + 4 > quint64(b.size())) throw Error("Effect directory is outside the ROM");
    const auto length = u32(b, directory);
    if (length % 4 || quint64(id) * 4 + 4 > length || directory + 4 + length > quint64(b.size()))
        throw Error("Effect resource is outside its directory");
    const quint64 at = 0x594280ULL + u32(b, directory + 4 + id * 4);
    if (at + 8 > quint64(b.size())) throw Error("Effect resource is outside the ROM");
    const auto stored = u32(b, at), decoded = u32(b, at + 4);
    if (stored < 4 || at + 4 + stored > quint64(b.size()) || decoded > 16 * 1024 * 1024)
        throw Error("Effect resource exceeds its allocation");
    return decodeLzss(b.mid(at + 8, stored - 4), decoded);
}
void validateEffectArtwork(const Rom &rom, const AttackVisual &visual) {
    for (auto i = visual.artwork.begin(); i != visual.artwork.end(); ++i) {
        const auto original = retailEffectResource(rom, i.key() >> 16, i.key() & 65535);
        if (original.size() != i->rgba.size() || sha256(original) != i->originalHash)
            throw Error("Replacement effect artwork does not match this ROM's source texture");
    }
}
}
