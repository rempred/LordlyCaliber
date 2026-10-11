#include "engine/class_text.h"
#include "engine/class_data.h"
#include "engine/promotion.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace lordly {
static QJsonArray owners(const QString &kind = "owners", bool items = false) {
    QFile file(items ? ":/game/items/text-owners.json" : ":/game/classes/text-owners.json");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing class description resource catalog");
    return QJsonDocument::fromJson(file.readAll()).object()[kind].toArray();
}
void verifyClassDescriptionRouting(const QByteArray &image, quint32 entry) {
    for (const auto &value : owners("routingOwners")) {
        const auto owner = value.toObject();
        auto bytes = image.mid(owner["start"].toInt(), owner["size"].toInt());
        if (bytes.size() != owner["size"].toInt())
            throw Error("Truncated class description routing owner");
        if (entry) {
            if (owner["entry"].toBool()) {
                if (u32(bytes, 0) != (0x08000000U | ((entry >> 2) & 0x3ffffff)) || u32(bytes, 4))
                    throw Error("Class description routing entry has changed");
                put32(bytes, 0, 0x27bdff78);
                put32(bytes, 4, 0xafb60080);
            } else {
                const auto at = owner["immediate"].toInt();
                if (u32(bytes, at) != 0x24040100)
                    throw Error("Special class description routing has changed");
                put32(bytes, at, 0x240400e0);
            }
        }
        if (sha256(bytes) != owner["retailSha256"].toString().toLatin1())
            throw Error("Unrecognized class description routing owner");
    }
}
void installClassDescriptionRouting(QByteArray &image, quint32 previousEntry, quint32 nextEntry) {
    verifyClassDescriptionRouting(image, previousEntry);
    if (!nextEntry || nextEntry % 4)
        throw Error("Invalid class description routing entry");
    for (const auto &value : owners("routingOwners")) {
        const auto owner = value.toObject();
        const auto start = owner["start"].toInt();
        if (owner["entry"].toBool()) {
            put32(image, start, 0x08000000U | ((nextEntry >> 2) & 0x3ffffff));
            put32(image, start + 4, 0);
        } else {
            put32(image, start + owner["immediate"].toInt(), 0x24040100);
        }
    }
    verifyClassDescriptionRouting(image, nextEntry);
}
static QByteArray relocate(const QJsonObject &owner, QByteArray bytes, quint32 before,
                           quint32 after) {
    if (bytes.size() != owner["size"].toInt())
        throw Error("Truncated description resource owner");
    if (owner.contains("word")) {
        const auto at = owner["word"].toInt();
        if (u32(bytes, at) != before)
            throw Error("Description resource table changed outside its rebuild");
        put32(bytes, at, after);
    } else {
        const auto high = owner["high"].toInt(), low = owner["low"].toInt();
        if (u32(bytes, high) != (0x3c050000U | (before >> 16)) ||
            u32(bytes, low) != (0x34a50000U | (before & 65535)))
            throw Error("Help resource reference changed outside its rebuild");
        put32(bytes, high, 0x3c050000U | (after >> 16));
        put32(bytes, low, 0x34a50000U | (after & 65535));
    }
    return bytes;
}
static void verifyOwners(const Rom &rom, quint32 key, bool items) {
    const auto classes = ClassPlacement::read(rom);
    for (const auto &value : owners("owners", items)) {
        const auto owner = value.toObject();
        const auto start = owner["start"].toInt();
        auto bytes = rom.bytes().mid(start, owner["size"].toInt());
        if (classes)
            bytes = classes->canonicalReferences(bytes, start);
        bytes =
            relocate(owner, bytes, key,
                     items ? ClassTextPlacement::itemDescriptorOffset - ClassTextPlacement::keyBase
                           : ClassTextPlacement::retailKey);
        const auto other = rom.bytes().mid(items ? ClassTextPlacement::descriptorOffset
                                                 : ClassTextPlacement::itemDescriptorOffset,
                                           ClassTextPlacement::descriptorSize);
        if (other.startsWith("LC05TEXT"))
            for (const auto &otherValue : owners("owners", !items)) {
                const auto sibling = otherValue.toObject();
                if (sibling["start"].toInt() == start)
                    bytes = relocate(sibling, bytes, u32(other, 12) - ClassTextPlacement::keyBase,
                                     (items ? ClassTextPlacement::descriptorOffset
                                            : ClassTextPlacement::itemDescriptorOffset) -
                                         ClassTextPlacement::keyBase);
            }
        if (sha256(bytes) != owner["retailSha256"].toString().toLatin1())
            throw Error("Unrecognized description resource owner");
    }
}
QByteArray ClassTextPlacement::canonicalReferences(const QByteArray &bytes, qsizetype start) const {
    for (const auto &value : owners("owners", items)) {
        const auto owner = value.toObject();
        if (owner["start"].toInt() == start)
            return relocate(owner, bytes, offset - keyBase,
                            items ? itemDescriptorOffset - keyBase : retailKey);
    }
    return bytes;
}
std::optional<ClassTextPlacement> ClassTextPlacement::read(const Rom &rom, bool items) {
    const auto header =
        rom.bytes().mid(items ? itemDescriptorOffset : descriptorOffset, descriptorSize);
    if (!header.startsWith("LC05TEXT"))
        return std::nullopt;
    if (rom.revision() != 0 || header.size() != descriptorSize || u32(header, 8) != 1 ||
        u32(header, 60) != crc32(header.first(60)) || header.mid(36, 24) != QByteArray(24, 0))
        throw Error("Damaged description placement descriptor");
    ClassTextPlacement result{u32(header, 12), u32(header, 16), u32(header, 20), u32(header, 24),
                              items};
    if (result.offset < 0x2800000 || result.offset % 16 || result.allocation < 16 ||
        result.allocation % 16 || quint64(result.offset) + result.allocation > 0x4000000 ||
        quint64(result.offset) + result.allocation > quint64(rom.bytes().size()) ||
        result.decodedSize > 0x80000 || result.count < (items ? 278U : 225U) ||
        result.count > (items ? 511U : 316U))
        throw Error("Description placement exceeds its bounds");
    const auto bytes = rom.bytes().mid(result.offset, result.allocation);
    if (crc32(bytes) != u32(header, 28) || u32(bytes, 0) != result.allocation - 4 ||
        u32(bytes, 4) != result.decodedSize)
        throw Error("Relocated descriptions changed outside their rebuild");
    const auto resource = Resource::load(rom, result.offset, result.allocation);
    if (crc32(resource.decoded) != u32(header, 32) ||
        resource.decoded.count("\r\n") != result.count)
        throw Error("Relocated description contents disagree with their descriptor");
    verifyOwners(rom, result.offset - keyBase, items);
    return result;
}
QByteArray linkClassText(const Rom &rom, const QByteArray &compiled, int count, bool items) {
    rom.requireSupported();
    if (rom.revision() != 0 || count < (items ? 278 : 225) || count > (items ? 511 : 316) ||
        compiled.size() > 0x80000 || compiled.count("\r\n") != count || !compiled.endsWith("\r\n"))
        throw Error("Unsupported description bank");
    const auto prior = ClassTextPlacement::read(rom, items);
    const auto descriptorOffset =
        items ? ClassTextPlacement::itemDescriptorOffset : ClassTextPlacement::descriptorOffset;
    const auto retailKey = descriptorOffset - ClassTextPlacement::keyBase;
    const auto previousKey = prior ? prior->offset - ClassTextPlacement::keyBase : retailKey;
    verifyOwners(rom, previousKey, items);
    const auto stream = encodeLzss(compiled);
    const auto allocation = quint32((stream.size() + 8 + 15) & ~qsizetype(15));
    const bool tail =
        prior && quint64(prior->offset) + prior->allocation == quint64(rom.bytes().size());
    const auto offset = prior && (allocation <= prior->allocation || tail)
                            ? prior->offset
                            : quint32((rom.bytes().size() + 15) & ~qsizetype(15));
    if (offset < 0x2800000 || quint64(offset) + allocation > 0x4000000)
        throw Error("Descriptions exceed the cartridge address range");
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
    image.replace(descriptorOffset, descriptor.size(), descriptor);
    for (const auto &value : owners("owners", items)) {
        const auto owner = value.toObject();
        const auto start = owner["start"].toInt(), size = owner["size"].toInt();
        image.replace(start, size,
                      relocate(owner, image.mid(start, size), previousKey,
                               offset - ClassTextPlacement::keyBase));
    }
    const auto reopened = Rom::fromBytes(image);
    const auto placement = ClassTextPlacement::read(reopened, items);
    if (!placement || Resource::load(reopened, offset, allocation).decoded != compiled)
        throw Error("Relocated description readback failed");
    return image;
}
QByteArray ClassTextPlacement::canonicalAll(const Rom &rom, QByteArray bytes, qsizetype start) {
    for (const bool items : {false, true}) {
        const auto p = read(rom, items);
        if (p)
            bytes = p->canonicalReferences(bytes, start);
    }
    return bytes;
}
} // namespace lordly
