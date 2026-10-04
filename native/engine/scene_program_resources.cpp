#include "engine/scene_program_resources.h"
#include "engine/promotion.h"
#include <QSet>
namespace lordly {
namespace {
constexpr quint32 base = 0x594280;
quint32 align(quint32 value) {
    return (value + 15U) & ~15U;
}
QVector<quint32> referenceWords(const Rom &rom) {
    QVector<quint32> result;
    // Top-level Director, continuation and parent event directories.
    for (const quint32 root : {0x9398e8U, 0x1e24c1eU, 0x1f3ca84U}) {
        const auto extent = u32(rom.bytes(), root);
        if (!extent || extent % 4 || extent > 262144 ||
            quint64(root) + 4 + extent > quint64(rom.bytes().size()))
            throw Error("Scene resource directory exceeds the ROM");
        for (quint32 at = root + 4; at < root + 4 + extent; at += 4)
            result << at;
    }
    return result;
}
QVector<quint32> references(const Rom &rom, quint32 key) {
    QVector<quint32> result;
    for (const auto word : referenceWords(rom))
        if (u32(rom.bytes(), word) == key)
            result << word;
    return result;
}
} // namespace
std::optional<SceneProgramPlacement> SceneProgramResources::placement(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Scene rebuild requires US revision zero");
    const auto header = rom.bytes().mid(descriptorOffset, descriptorSize);
    if (header == QByteArray(descriptorSize, char(-1)))
        return {};
    if (header.size() != descriptorSize || header.first(8) != "LC05SCPT" || u32(header, 8) != 1 ||
        header.mid(28, 64) != QByteArray(64, 0) || u32(header, 92) != crc32(header.first(92)))
        throw Error("Scene allocation descriptor is damaged or occupied");
    SceneProgramPlacement result{u32(header, 12), u32(header, 16), {}};
    const auto count = u32(header, 20);
    if (!count || count > 65536 || result.offset < 0x2800000 || result.offset % 16 ||
        !result.allocation || result.allocation % 16 ||
        quint64(result.offset) + result.allocation > 0x4000000 ||
        quint64(result.offset) + result.allocation > quint64(rom.bytes().size()) ||
        count * 20 > result.allocation)
        throw Error("Scene allocation exceeds the cartridge");
    const auto data = rom.bytes().mid(result.offset, result.allocation);
    if (crc32(data) != u32(header, 24))
        throw Error("Scene allocation payload is damaged");
    const auto allowed = referenceWords(rom);
    const QSet<quint32> allowedSet(allowed.begin(), allowed.end());
    QSet<quint32> claimed;
    quint32 referenceCursor = count * 20, previous = 0;
    for (quint32 i = 0; i < count; ++i) {
        const auto owner = u32(data, i * 20), offset = u32(data, i * 20 + 4),
                   size = u32(data, i * 20 + 8), refs = u32(data, i * 20 + 12),
                   length = u32(data, i * 20 + 16);
        if ((i && owner <= previous) || refs != referenceCursor || !length ||
            quint64(refs) + quint64(length) * 4 > quint64(data.size()) || size < 8 ||
            quint64(offset) + size > result.allocation || u32(data, offset) + quint64(4) != size)
            throw Error("Scene allocation has invalid members");
        SceneProgramMember member{offset, size, {}};
        for (quint32 r = 0; r < length; ++r) {
            const auto word = u32(data, refs + r * 4);
            if (!allowedSet.contains(word) || claimed.contains(word) || (!r && word != owner) ||
                (r && word <= member.references.last()) ||
                u32(rom.bytes(), word) != result.offset + offset - base)
                throw Error("Scene allocation resource links differ from their owner");
            member.references << word;
            claimed.insert(word);
        }
        result.members[owner] = std::move(member);
        referenceCursor += length * 4;
        previous = owner;
    }
    quint32 cursor = align(referenceCursor);
    for (const auto &member : result.members) {
        if (member.offset != cursor)
            throw Error("Scene allocation members overlap metadata or one another");
        cursor = align(cursor + member.size);
    }
    if (cursor != result.allocation)
        throw Error("Scene allocation has an unowned trailing region");
    for (const auto word : allowed) {
        const quint64 target = quint64(u32(rom.bytes(), word)) + base;
        if (target >= result.offset && target < quint64(result.offset) + result.allocation &&
            !claimed.contains(word))
            throw Error("Unowned resource link enters the scene allocation");
    }
    return result;
}
QByteArray SceneProgramResources::rebuild(const Rom &rom,
                                          const QMap<quint32, QByteArray> &compiled) {
    const auto prior = placement(rom);
    if (compiled.isEmpty())
        return rom.bytes();
    struct Member {
        QByteArray bytes;
        QVector<quint32> references;
    };
    QMap<quint32, Member> members;
    if (prior)
        for (auto it = prior->members.cbegin(); it != prior->members.cend(); ++it)
            members[it.key()] = {rom.bytes().mid(prior->offset + it->offset, it->size),
                                 it->references};
    const auto sources = Directors::load(rom);
    for (auto it = compiled.cbegin(); it != compiled.cend(); ++it) {
        if (!sources.entries().contains(it.key()) && !sources.continuations().contains(it.key()))
            throw Error("Rebuilt script is not a current Director resource");
        const auto parsed = DirectorProgram::decode(it.value());
        const auto &original = sources.entries().contains(it.key())
                                   ? sources.entries()[it.key()]
                                   : sources.continuations()[it.key()];
        if (parsed.resourceClass() != original.program.resourceClass() ||
            parsed.commands.last().opcode != 0x80000001)
            throw Error("Rebuilt script changed its terminal resource class");
        const auto links = references(rom, it.key());
        if (links.isEmpty())
            throw Error("Scene resource has no directory owner");
        const auto at = base + it.key(), size = u32(rom.bytes(), at),
                   decoded = u32(rom.bytes(), at + 4);
        qsizetype consumed = 0;
        decodeLzss(rom.bytes().mid(at + 8, size - 4), decoded, &consumed);
        const auto packed = encodeLzss(it.value());
        // Retain padding or unrecognized bytes after the old compressed stream.
        const auto trailing = rom.bytes().mid(at + 8 + consumed, size - 4 - consumed);
        QByteArray envelope(8, 0);
        put32(envelope, 0, quint32(4 + packed.size() + trailing.size()));
        put32(envelope, 4, quint32(it.value().size()));
        envelope += packed;
        envelope += trailing;
        members[links.first()] = {envelope, links};
    }
    quint32 refsSize = 0;
    for (const auto &member : members)
        refsSize += quint32(member.references.size() * 4);
    quint32 refsCursor = quint32(members.size() * 20);
    QByteArray payload(align(refsCursor + refsSize), char(-1));
    int row = 0;
    for (auto it = members.cbegin(); it != members.cend(); ++it, ++row) {
        put32(payload, row * 20, it.key());
        put32(payload, row * 20 + 4, quint32(payload.size()));
        put32(payload, row * 20 + 8, quint32(it->bytes.size()));
        put32(payload, row * 20 + 12, refsCursor);
        put32(payload, row * 20 + 16, quint32(it->references.size()));
        for (const auto word : it->references) {
            put32(payload, refsCursor, word);
            refsCursor += 4;
        }
        payload += it->bytes;
        payload += QByteArray(align(quint32(payload.size())) - payload.size(), char(-1));
    }
    const quint32 allocation = quint32(payload.size());
    const bool tail =
        prior && quint64(prior->offset) + prior->allocation == quint64(rom.bytes().size());
    const quint32 offset = prior && (allocation <= prior->allocation || tail)
                               ? prior->offset
                               : align(quint32(rom.bytes().size()));
    if (offset < 0x2800000 || quint64(offset) + allocation > 0x4000000)
        throw Error("Rebuilt scenes exceed the cartridge");
    auto image = rom.bytes();
    if (prior)
        image.replace(prior->offset, prior->allocation, QByteArray(prior->allocation, char(-1)));
    if (tail && offset == prior->offset)
        image.resize(offset);
    if (quint64(image.size()) < quint64(offset) + allocation)
        image += QByteArray(offset + allocation - image.size(), char(-1));
    image.replace(offset, allocation, payload);
    row = 0;
    for (const auto &member : members) {
        for (const auto word : member.references)
            put32(image, word, offset + u32(payload, row * 20 + 4) - base);
        ++row;
    }
    QByteArray header(descriptorSize, 0);
    header.replace(0, 8, "LC05SCPT");
    put32(header, 8, 1);
    put32(header, 12, offset);
    put32(header, 16, allocation);
    put32(header, 20, quint32(members.size()));
    put32(header, 24, crc32(payload));
    put32(header, 92, crc32(header.first(92)));
    image.replace(descriptorOffset, descriptorSize, header);
    const auto reopened = Rom::fromBytes(image);
    placement(reopened);
    const auto check = Directors::load(reopened);
    for (auto it = compiled.cbegin(); it != compiled.cend(); ++it) {
        const auto links = references(rom, it.key());
        const auto key = u32(image, links.first());
        const auto &program = check.entries().contains(key) ? check.entries()[key].program
                                                            : check.continuations()[key].program;
        if (program.source != it.value())
            throw Error("Rebuilt scene failed decoded readback");
    }
    return image;
}
} // namespace lordly
