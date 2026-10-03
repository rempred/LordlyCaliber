#include "engine/class_text.h"
#include "engine/class_data.h"
#include "engine/promotion.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace lordly {
static QJsonArray owners() {
    QFile file(":/game/classes/text-owners.json");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing class description resource catalog");
    return QJsonDocument::fromJson(file.readAll()).object()["owners"].toArray();
}
static QByteArray relocate(const QJsonObject &owner, QByteArray bytes, quint32 before,
                           quint32 after) {
    if (bytes.size() != owner["size"].toInt())
        throw Error("Truncated class description resource owner");
    if (owner.contains("word")) {
        const auto at = owner["word"].toInt();
        if (u32(bytes, at) != before)
            throw Error("Class description resource table changed outside its rebuild");
        put32(bytes, at, after);
    } else {
        const auto high = owner["high"].toInt(), low = owner["low"].toInt();
        if (u32(bytes, high) != (0x3c050000U | (before >> 16)) ||
            u32(bytes, low) != (0x34a50000U | (before & 65535)))
            throw Error("Class help resource reference changed outside its rebuild");
        put32(bytes, high, 0x3c050000U | (after >> 16));
        put32(bytes, low, 0x34a50000U | (after & 65535));
    }
    return bytes;
}
static void verifyOwners(const Rom &rom, quint32 key) {
    const auto classes = ClassPlacement::read(rom);
    for (const auto &value : owners()) {
        const auto owner = value.toObject();
        const auto start = owner["start"].toInt();
        auto bytes = rom.bytes().mid(start, owner["size"].toInt());
        if (classes)
            bytes = classes->canonicalReferences(bytes, start);
        bytes = relocate(owner, bytes, key, ClassTextPlacement::retailKey);
        if (sha256(bytes) != owner["retailSha256"].toString().toLatin1())
            throw Error("Unrecognized class description resource owner");
    }
}
QByteArray ClassTextPlacement::canonicalReferences(const QByteArray &bytes, qsizetype start) const {
    for (const auto &value : owners()) {
        const auto owner = value.toObject();
        if (owner["start"].toInt() == start)
            return relocate(owner, bytes, offset - keyBase, retailKey);
    }
    return bytes;
}
std::optional<ClassTextPlacement> ClassTextPlacement::read(const Rom &rom) {
    const auto header = rom.bytes().mid(descriptorOffset, descriptorSize);
    if (!header.startsWith("LC05TEXT"))
        return std::nullopt;
    if (rom.revision() != 0 || header.size() != descriptorSize || u32(header, 8) != 1 ||
        u32(header, 60) != crc32(header.first(60)) || header.mid(36, 24) != QByteArray(24, 0))
        throw Error("Damaged class description placement descriptor");
    ClassTextPlacement result{u32(header, 12), u32(header, 16), u32(header, 20), u32(header, 24)};
    if (result.offset < 0x2800000 || result.offset % 16 || result.allocation < 16 ||
        result.allocation % 16 || quint64(result.offset) + result.allocation > 0x4000000 ||
        quint64(result.offset) + result.allocation > quint64(rom.bytes().size()) ||
        result.decodedSize > 0x80000 || result.count < 225 || result.count > 316)
        throw Error("Class description placement exceeds its bounds");
    const auto bytes = rom.bytes().mid(result.offset, result.allocation);
    if (crc32(bytes) != u32(header, 28) || u32(bytes, 0) != result.allocation - 4 ||
        u32(bytes, 4) != result.decodedSize)
        throw Error("Relocated class descriptions changed outside their rebuild");
    const auto resource = Resource::load(rom, result.offset, result.allocation);
    if (crc32(resource.decoded) != u32(header, 32) ||
        resource.decoded.count("\r\n") != result.count)
        throw Error("Relocated class description contents disagree with their descriptor");
    verifyOwners(rom, result.offset - keyBase);
    return result;
}
QByteArray linkClassText(const Rom &rom, const QByteArray &compiled, int count) {
    rom.requireSupported();
    if (rom.revision() != 0 || count < 225 || count > 316 || compiled.size() > 0x80000 ||
        compiled.count("\r\n") != count || !compiled.endsWith("\r\n"))
        throw Error("Unsupported class description bank");
    const auto prior = ClassTextPlacement::read(rom);
    const auto previousKey =
        prior ? prior->offset - ClassTextPlacement::keyBase : ClassTextPlacement::retailKey;
    verifyOwners(rom, previousKey);
    const auto stream = encodeLzss(compiled);
    const auto allocation = quint32((stream.size() + 8 + 15) & ~qsizetype(15));
    const bool tail =
        prior && quint64(prior->offset) + prior->allocation == quint64(rom.bytes().size());
    const auto offset = prior && (allocation <= prior->allocation || tail)
                            ? prior->offset
                            : quint32((rom.bytes().size() + 15) & ~qsizetype(15));
    if (offset < 0x2800000 || quint64(offset) + allocation > 0x4000000)
        throw Error("Class descriptions exceed the cartridge address range");
    auto image = rom.bytes();
    if (prior)
        image.replace(prior->offset, prior->allocation, QByteArray(prior->allocation, char(-1)));
    if (tail && offset == prior->offset)
        image.resize(offset);
    if (quint64(image.size()) < quint64(offset) + allocation)
        image.append(QByteArray(offset + allocation - image.size(), char(-1)));
    QByteArray container(allocation, 3);
    put32(container, 0, allocation - 4);
    put32(container, 4, quint32(compiled.size()));
    container.replace(8, stream.size(), stream);
    image.replace(offset, allocation, container);
    QByteArray descriptor(ClassTextPlacement::descriptorSize, 0);
    descriptor.replace(0, 8, "LC05TEXT");
    put32(descriptor, 8, 1);
    put32(descriptor, 12, offset);
    put32(descriptor, 16, allocation);
    put32(descriptor, 20, quint32(compiled.size()));
    put32(descriptor, 24, count);
    put32(descriptor, 28, crc32(container));
    put32(descriptor, 32, crc32(compiled));
    put32(descriptor, 60, crc32(descriptor.first(60)));
    image.replace(ClassTextPlacement::descriptorOffset, descriptor.size(), descriptor);
    for (const auto &value : owners()) {
        const auto owner = value.toObject();
        const auto start = owner["start"].toInt(), size = owner["size"].toInt();
        image.replace(start, size,
                      relocate(owner, image.mid(start, size), previousKey,
                               offset - ClassTextPlacement::keyBase));
    }
    const auto reopened = Rom::fromBytes(image);
    const auto placement = ClassTextPlacement::read(reopened);
    if (!placement || Resource::load(reopened, offset, allocation).decoded != compiled)
        throw Error("Relocated class description readback failed");
    return image;
}
} // namespace lordly
