#include "engine/dialogue_resources.h"
#include "engine/promotion.h"
namespace lordly {
namespace {
constexpr quint32 base = 0x594280;
quint32 align(quint32 n) {
    return (n + 15U) & ~15U;
}
quint32 root(quint32 identity) {
    if (identity > 0x1ffff)
        throw Error("Invalid dialogue resource identity");
    return (identity & 0x10000 ? 0x21b8ba4U : 0x1a3b7b2U) + base;
}
quint32 pointer(const Rom &rom, quint32 identity) {
    const auto table = root(identity);
    const auto size = u32(rom.bytes(), table), index = identity & 65535;
    if (size % 4 || index >= size / 4)
        throw Error("Dialogue selector exceeds its resource directory");
    return table + 4 + index * 4;
}
QByteArray directories(const Rom &rom) {
    QByteArray result;
    for (const auto table : {DialogueArchive::Table::Primary, DialogueArchive::Table::Alternate}) {
        const auto keys = DialogueArchive::directory(rom, table);
        for (const auto key : keys) {
            QByteArray word(4, 0);
            put32(word, 0, key);
            result += word;
        }
    }
    return result;
}
QByteArray envelope(const QByteArray &payload) {
    QByteArray result(4, 0);
    put32(result, 0, quint32(payload.size()));
    return result + payload;
}
} // namespace
std::optional<DialoguePlacement> DialogueResources::placement(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Dialogue resource rebuild requires US revision zero");
    const auto header = rom.bytes().mid(descriptorOffset, descriptorSize);
    if (header == QByteArray(descriptorSize, char(-1)))
        return std::nullopt;
    if (header.size() != descriptorSize || header.first(8) != "LC05DLOG" || u32(header, 8) != 1 ||
        header.mid(40, 52) != QByteArray(52, 0) || crc32(header.first(92)) != u32(header, 92))
        throw Error("Dialogue allocation descriptor is damaged or occupied");
    DialoguePlacement result{u32(header, 12), u32(header, 16), {}};
    const auto count = u32(header, 20);
    if (!count || count > 504 || result.offset < 0x2800000 || result.offset % 16 ||
        !result.allocation || result.allocation % 16 ||
        quint64(result.offset) + result.allocation > 0x4000000 ||
        quint64(result.offset) + result.allocation > quint64(rom.bytes().size()) ||
        count * 12 > result.allocation)
        throw Error("Dialogue allocation exceeds the cartridge");
    const auto data = rom.bytes().mid(result.offset, result.allocation);
    if (crc32(data) != u32(header, 24) || crc32(directories(rom)) != u32(header, 28) ||
        u32(header, 32) != quint32(DialogueArchive::directory(rom).size()) ||
        u32(header, 36) !=
            quint32(DialogueArchive::directory(rom, DialogueArchive::Table::Alternate).size()))
        throw Error("Dialogue resources differ from their allocation descriptor");
    quint32 cursor = align(count * 12), previous = 0;
    for (quint32 i = 0; i < count; ++i) {
        const auto identity = u32(data, i * 12), offset = u32(data, i * 12 + 4),
                   size = u32(data, i * 12 + 8);
        if ((i && identity <= previous) || offset != cursor || size < 4 ||
            quint64(offset) + size > result.allocation || u32(data, offset) + 4 != size ||
            u32(rom.bytes(), pointer(rom, identity)) != result.offset + offset - base)
            throw Error("Dialogue allocation has invalid members or resource links");
        result.members[identity] = {offset, size};
        previous = identity;
        cursor = align(offset + size);
    }
    if (cursor != result.allocation)
        throw Error("Dialogue allocation has an unowned trailing region");
    // A link into owned space must be accounted for before that space can be reused.
    for (const auto table : {DialogueArchive::Table::Primary, DialogueArchive::Table::Alternate}) {
        const auto keys = DialogueArchive::directory(rom, table);
        for (int i = 0; i < keys.size(); ++i) {
            const quint64 at = quint64(keys[i]) + base;
            if (at >= result.offset && at < quint64(result.offset) + result.allocation &&
                !result.members.contains(Dialogues::identity(table, i)))
                throw Error("An unowned dialogue selector points into the allocation");
        }
    }
    return result;
}
QByteArray DialogueResources::rebuild(const Rom &rom, const QMap<quint32, QByteArray> &compiled) {
    const auto prior = placement(rom);
    if (compiled.isEmpty())
        return rom.bytes();
    QMap<quint32, QByteArray> members;
    if (prior)
        for (auto it = prior->members.cbegin(); it != prior->members.cend(); ++it)
            members[it.key()] = rom.bytes().mid(prior->offset + it->first, it->second);
    for (auto it = compiled.cbegin(); it != compiled.cend(); ++it) {
        DialogueArchive::fromDecoded(it.value());
        const auto key = u32(rom.bytes(), pointer(rom, it.key()));
        if (!key || key % 2 || key >= 0x10000000)
            throw Error("Edited dialogue archive has no resource");
        const quint32 at = key + base, extent = u32(rom.bytes(), at);
        if (extent > 16 * 1024 * 1024 || quint64(at) + 4 + extent > quint64(rom.bytes().size()))
            throw Error("Dialogue source archive exceeds its envelope");
        const auto stored = rom.bytes().mid(at + 4, extent);
        auto member = LhaMember::read(stored, 0);
        const auto end = member.headerSize + member.storedSize;
        if (end > stored.size())
            throw Error("Dialogue source member exceeds its envelope");
        member.offset = at + 4;
        members[it.key()] = envelope(member.rebuild(rom, it.value()) + stored.mid(end));
    }
    QByteArray payload(align(quint32(members.size() * 12)), char(-1));
    int row = 0;
    for (auto it = members.cbegin(); it != members.cend(); ++it, ++row) {
        put32(payload, row * 12, it.key());
        put32(payload, row * 12 + 4, quint32(payload.size()));
        put32(payload, row * 12 + 8, quint32(it->size()));
        payload += it.value();
        payload += QByteArray(align(quint32(payload.size())) - payload.size(), char(-1));
    }
    const quint32 allocation = quint32(payload.size());
    const bool tail =
        prior && quint64(prior->offset) + prior->allocation == quint64(rom.bytes().size());
    const quint32 offset = prior && (allocation <= prior->allocation || tail)
                               ? prior->offset
                               : align(quint32(rom.bytes().size()));
    if (offset < 0x2800000 || quint64(offset) + allocation > 0x4000000)
        throw Error("Rebuilt dialogue resources exceed the cartridge");
    auto image = rom.bytes();
    if (prior)
        image.replace(prior->offset, prior->allocation, QByteArray(prior->allocation, char(-1)));
    if (tail && offset == prior->offset)
        image.resize(offset);
    if (quint64(image.size()) < quint64(offset) + allocation)
        image += QByteArray(offset + allocation - image.size(), char(-1));
    image.replace(offset, allocation, payload);
    for (int i = 0; i < members.size(); ++i)
        put32(image, pointer(rom, u32(payload, i * 12)), offset + u32(payload, i * 12 + 4) - base);
    QByteArray header(descriptorSize, 0);
    header.replace(0, 8, "LC05DLOG");
    put32(header, 8, 1);
    put32(header, 12, offset);
    put32(header, 16, allocation);
    put32(header, 20, quint32(members.size()));
    put32(header, 24, crc32(payload));
    put32(header, 28, crc32(directories(Rom::fromBytes(image))));
    put32(header, 32, quint32(DialogueArchive::directory(rom).size()));
    put32(header, 36,
          quint32(DialogueArchive::directory(rom, DialogueArchive::Table::Alternate).size()));
    put32(header, 92, crc32(header.first(92)));
    image.replace(descriptorOffset, descriptorSize, header);
    const auto reopened = Rom::fromBytes(image);
    placement(reopened);
    for (auto it = compiled.cbegin(); it != compiled.cend(); ++it)
        if (DialogueArchive::load(reopened, int(it.key() & 65535),
                                  it.key() & 0x10000 ? DialogueArchive::Table::Alternate
                                                     : DialogueArchive::Table::Primary)
                .bytes() != it.value())
            throw Error("Rebuilt dialogue archive failed readback");
    return image;
}
} // namespace lordly
