#include "engine/location_text.h"
#include "engine/promotion.h"

namespace lordly {
std::optional<LocationTextPlacement> LocationTextPlacement::read(const Rom &rom) {
    if (rom.revision()) return std::nullopt;
    const auto header = rom.bytes().mid(descriptorOffset, descriptorSize);
    if (!header.startsWith("LCLOCT01")) {
        if (u32(rom.bytes(), directoryWord) != descriptorOffset - keyBase)
            throw Error("Unrecognized location description resource reference");
        return std::nullopt;
    }
    if (header.size() != descriptorSize || u32(header, 8) != 1 ||
        u32(header, 24) != recordCount || header.mid(36, 24) != QByteArray(24, 0) ||
        u32(header, 60) != crc32(header.first(60)))
        throw Error("Damaged location description placement");
    const LocationTextPlacement p{u32(header, 12), u32(header, 16), u32(header, 20)};
    if (p.offset < 0x2800000 || p.offset % 16 || p.allocation < 16 || p.allocation % 16 ||
        quint64(p.offset) + p.allocation > 0x4000000 ||
        quint64(p.offset) + p.allocation > quint64(rom.bytes().size()) ||
        p.decodedSize < recordCount * 2 || p.decodedSize > recordCount * 0x500 ||
        u32(rom.bytes(), directoryWord) != p.offset - keyBase)
        throw Error("Location descriptions exceed their allocation or reference");
    const auto bytes = rom.bytes().mid(p.offset, p.allocation);
    if (u32(bytes, 0) != p.allocation - 4 || u32(bytes, 4) != p.decodedSize ||
        crc32(bytes) != u32(header, 28))
        throw Error("Relocated location descriptions changed outside their rebuild");
    const auto resource = Resource::load(rom, p.offset, p.allocation);
    if (crc32(resource.decoded) != u32(header, 32) ||
        resource.decoded.count("\r\n") != recordCount || !resource.decoded.endsWith("\r\n"))
        throw Error("Location description contents disagree with their placement");
    return p;
}
QByteArray LocationTextPlacement::canonicalReferences(const Rom &rom, QByteArray bytes,
                                                     qsizetype start) {
    if (rom.revision() || start > directoryWord ||
        start + bytes.size() < directoryWord + 4) return bytes;
    const auto p = read(rom);
    if (p) {
        if (u32(bytes, directoryWord - start) != p->offset - keyBase)
            throw Error("Location description directory changed outside its rebuild");
        put32(bytes, directoryWord - start, descriptorOffset - keyBase);
    }
    return bytes;
}
QByteArray linkLocationText(const Rom &rom, const QByteArray &compiled) {
    rom.requireSupported();
    if (rom.revision() || compiled.count("\r\n") != LocationTextPlacement::recordCount ||
        !compiled.endsWith("\r\n") || compiled.size() > LocationTextPlacement::recordCount * 0x500)
        throw Error("Unsupported location description bank");
    const auto prior = LocationTextPlacement::read(rom);
    const auto stream = encodeLzss(compiled);
    if (decodeLzss(stream, compiled.size()) != compiled)
        throw Error("Location description compression readback failed");
    const quint32 allocation = (stream.size() + 8 + 15) & ~qsizetype(15);
    const bool tail = prior && quint64(prior->offset) + prior->allocation == quint64(rom.bytes().size());
    const quint32 offset = prior && (allocation <= prior->allocation || tail)
        ? prior->offset : (rom.bytes().size() + 15) & ~qsizetype(15);
    if (offset < 0x2800000 || quint64(offset) + allocation > 0x4000000)
        throw Error("Location descriptions exceed cartridge space");
    auto image = rom.bytes();
    if (prior) image.replace(prior->offset, prior->allocation, QByteArray(prior->allocation, char(-1)));
    if (tail && offset == prior->offset) image.resize(offset);
    if (quint64(image.size()) < quint64(offset) + allocation)
        image.append(QByteArray(offset + allocation - image.size(), char(-1)));
    QByteArray container(allocation, 3);
    put32(container, 0, allocation - 4);
    put32(container, 4, compiled.size());
    container.replace(8, stream.size(), stream);
    image.replace(offset, allocation, container);
    QByteArray header(LocationTextPlacement::descriptorSize, 0);
    header.replace(0, 8, "LCLOCT01");
    put32(header, 8, 1);
    put32(header, 12, offset);
    put32(header, 16, allocation);
    put32(header, 20, compiled.size());
    put32(header, 24, LocationTextPlacement::recordCount);
    put32(header, 28, crc32(container));
    put32(header, 32, crc32(compiled));
    put32(header, 60, crc32(header.first(60)));
    image.replace(LocationTextPlacement::descriptorOffset, header.size(), header);
    put32(image, LocationTextPlacement::directoryWord, offset - LocationTextPlacement::keyBase);
    const auto reopened = Rom::fromBytes(image);
    if (!LocationTextPlacement::read(reopened) ||
        Resource::load(reopened, offset, allocation).decoded != compiled)
        throw Error("Location description export readback failed");
    return image;
}
} // namespace lordly
