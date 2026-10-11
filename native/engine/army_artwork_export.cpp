#include "engine/army_artwork.h"
namespace lordly {
namespace {
QByteArray container(const QByteArray &decoded) {
    const auto compressed = encodeLzss(decoded);
    if (decodeLzss(compressed, decoded.size()) != decoded)
        throw Error("Army artwork compression failed readback");
    QByteArray result(8, 0);
    put32(result, 0, quint32(compressed.size() + 4));
    put32(result, 4, quint32(decoded.size()));
    return result + compressed;
}
quint32 bankStart(quint32 offset) {
    return ((offset - ArmyArtwork::resourceBase + 65535U) & ~65535U) + ArmyArtwork::resourceBase;
}
} // namespace
QByteArray ArmyArtwork::rebuild(const Rom &rom, const std::array<QByteArray, 6> &compiled) const {
    if (compiled != decodedPacks())
        throw Error("Compiled Army artwork differs from the project");
    if (!pixelsModified())
        return rom.bytes();
    if (rom.bytes().mid(descriptorOffset, descriptorSize) != descriptor_)
        throw Error("Another export owner changed the Army artwork descriptor");
    if (!placement_ && descriptor_ != QByteArray(descriptorSize, char(-1)))
        throw Error("Army artwork descriptor allocation is occupied");
    for (auto it = ownerWords_.cbegin(); it != ownerWords_.cend(); ++it)
        if (u32(rom.bytes(), it.key()) != it.value())
            throw Error("Another export owner changed an Army resource operand");
    if (placement_ &&
        QByteArray::fromHex(sha256(rom.bytes().mid(placement_->offset, placement_->allocation))) !=
            placement_->hash)
        throw Error("Another export owner changed the Army artwork allocation");
    QByteArray payload;
    Placement next;
    for (int atlas = 0; atlas < 6; ++atlas) {
        const quint32 alignment = atlas % 2 ? 16 : 65536;
        payload += QByteArray((alignment - payload.size() % alignment) % alignment, char(-1));
        next.relative[atlas] = quint32(payload.size());
        if (atlas % 2 && next.relative[atlas] >> 16 != next.relative[atlas - 1] >> 16)
            throw Error("The sprite pair cannot share the loader's upper address operand");
        const auto stored = container(compiled[atlas]);
        next.sizes[atlas] = quint32(stored.size());
        payload += stored;
    }
    payload += QByteArray((16 - payload.size() % 16) % 16, char(-1));
    next.allocation = quint32(payload.size());
    const bool tail = placement_ && quint64(placement_->offset) + placement_->allocation ==
                                        quint64(rom.bytes().size());
    next.offset = placement_ && (next.allocation <= placement_->allocation || tail)
                      ? placement_->offset
                      : bankStart(quint32(rom.bytes().size()));
    if (next.offset < 0x2800000 || quint64(next.offset) + next.allocation > 0x4000000)
        throw Error("Army artwork exceeds cartridge capacity");
    auto image = rom.bytes();
    if (placement_)
        image.replace(placement_->offset, placement_->allocation,
                      QByteArray(placement_->allocation, char(-1)));
    if (tail && next.offset == placement_->offset)
        image.resize(next.offset);
    if (quint64(image.size()) < quint64(next.offset) + next.allocation)
        image += QByteArray(next.offset + next.allocation - image.size(), char(-1));
    image.replace(next.offset, next.allocation, payload);
    QMap<int, quint32> patches;
    auto patch = [&](int at, quint32 value) {
        if (patches.contains(at) && patches[at] != value)
            throw Error("Army atlas keys disagree at a shared instruction");
        patches[at] = value;
    };
    for (int atlas = 0; atlas < 6; ++atlas) {
        const auto key = next.offset + next.relative[atlas] - resourceBase;
        for (const auto &[hi, lo] : specs()[atlas].sites) {
            patch(hi, (u32(image, hi) & 0xffff0000) | (key >> 16));
            patch(lo, (u32(image, lo) & 0xffff0000) | (key & 65535));
        }
    }
    for (auto it = patches.cbegin(); it != patches.cend(); ++it)
        put32(image, it.key(), it.value());
    QByteArray descriptor(descriptorSize, 0);
    descriptor.replace(0, 8, "LC05ARMY");
    put32(descriptor, 8, 1);
    put32(descriptor, 12, next.offset);
    put32(descriptor, 16, next.allocation);
    for (int atlas = 0; atlas < 6; ++atlas) {
        put32(descriptor, 32 + atlas * 4, next.relative[atlas]);
        put32(descriptor, 56 + atlas * 4, next.sizes[atlas]);
    }
    descriptor.replace(128, 32, QByteArray::fromHex(sha256(payload)));
    image.replace(descriptorOffset, descriptorSize, descriptor);
    const auto reopened = load(Rom::fromBytes(image));
    for (int atlas = 0; atlas < 6; ++atlas)
        if (reopened.packs_[atlas].source != compiled[atlas])
            throw Error("Rebuilt Army atlas differs after decompression");
    return image;
}
} // namespace lordly
