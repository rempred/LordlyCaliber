#include "engine/combat_artwork.h"
namespace lordly {
QByteArray CombatArtwork::rebuild(const Rom &rom, const QMap<QString, QByteArray> &compiled) const {
    if (compiled != decodedResources())
        throw Error("Compiled combat artwork differs from the project");
    if (!modified())
        return rom.bytes();
    if (rom.bytes().mid(descriptorOffset, descriptorSize) != descriptor_ ||
        (!allocationOffset_ && descriptor_ != QByteArray(descriptorSize, char(-1))))
        throw Error("Combat artwork descriptor is occupied or changed by another export owner");
    if (allocationOffset_ && rom.bytes().mid(allocationOffset_, allocationSize_) !=
                                 source_.mid(allocationOffset_, allocationSize_))
        throw Error("Another export owner changed the combat artwork allocation");
    auto resources = owned_;
    for (auto it = compiled.cbegin(); it != compiled.cend(); ++it)
        resources[it.key()] = it.value();
    QByteArray payload(resources.size() * 16, 0);
    QMap<QString, quint32> offsets;
    int index = 0;
    for (auto it = resources.cbegin(); it != resources.cend(); ++it, ++index) {
        const auto &row = binding(it.key());
        if (u32(rom.bytes(), row.memberOffset) != row.resourceKey)
            throw Error("Another export owner changed a combat descriptor member");
        const auto stored = encodeLzss(it.value());
        if (decodeLzss(stored, it->size()) != it.value())
            throw Error("Combat sprite compression failed readback");
        const auto relative = quint32(payload.size());
        offsets[it.key()] = relative;
        put32(payload, index * 16, row.descriptorKey);
        put32(payload, index * 16 + 4, quint32(row.art));
        put32(payload, index * 16 + 8, relative);
        put32(payload, index * 16 + 12, quint32(stored.size() + 8));
        QByteArray header(8, 0);
        put32(header, 0, quint32(stored.size() + 4));
        put32(header, 4, quint32(it->size()));
        payload += header + stored;
        payload += QByteArray((16 - payload.size() % 16) % 16, char(-1));
    }
    const bool tail = allocationOffset_ &&
                      quint64(allocationOffset_) + allocationSize_ == quint64(rom.bytes().size());
    const auto offset = allocationOffset_ && (payload.size() <= allocationSize_ || tail)
                            ? allocationOffset_
                            : (quint32(rom.bytes().size()) + 15) & ~15U;
    if (offset < 0x2800000 || quint64(offset) + payload.size() > 0x4000000)
        throw Error("Combat artwork exceeds cartridge capacity");
    auto image = rom.bytes();
    if (allocationOffset_)
        image.replace(allocationOffset_, allocationSize_, QByteArray(allocationSize_, char(-1)));
    if (tail && offset == allocationOffset_)
        image.resize(offset);
    if (quint64(image.size()) < quint64(offset) + payload.size())
        image += QByteArray(offset + payload.size() - image.size(), char(-1));
    image.replace(offset, payload.size(), payload);
    for (auto it = offsets.cbegin(); it != offsets.cend(); ++it)
        put32(image, binding(it.key()).memberOffset, offset + it.value() - resourceBase);
    QByteArray descriptor(descriptorSize, 0);
    descriptor.replace(0, 8, "LC05CMBA");
    put32(descriptor, 8, 1);
    put32(descriptor, 12, offset);
    put32(descriptor, 16, quint32(payload.size()));
    put32(descriptor, 20, quint32(resources.size()));
    descriptor.replace(64, 32, QByteArray::fromHex(sha256(payload)));
    image.replace(descriptorOffset, descriptorSize, descriptor);
    const auto reopened = load(Rom::fromBytes(image));
    if (reopened.owned_ != resources)
        throw Error("Rebuilt combat artwork differs after compressed readback");
    return image;
}
} // namespace lordly
