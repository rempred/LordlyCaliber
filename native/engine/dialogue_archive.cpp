#include "engine/dialogue_archive.h"
namespace lordly {
QVector<quint32> DialogueArchive::directory(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Dialogue archives currently require US revision zero");
    constexpr qsizetype at = 0x1a3b7b2 + 0x594280;
    const auto size = u32(rom.bytes(), at);
    if (!size || size % 4 || size > 65536 || quint64(at) + 4 + size > quint64(rom.bytes().size()))
        throw Error("Dialogue archive directory exceeds its ROM allocation");
    QVector<quint32> keys;
    for (quint32 i = 0; i < size; i += 4)
        keys << u32(rom.bytes(), at + 4 + i);
    return keys;
}
DialogueArchive DialogueArchive::load(const Rom &rom, int selector) {
    const auto keys = directory(rom);
    if (selector < 0 || selector >= keys.size())
        throw Error("Dialogue archive selector exceeds its directory");
    const auto key = keys[selector];
    if (!key || key >= 0x10000000 || key % 2)
        throw Error("Dialogue archive selector has no supported resource");
    const qsizetype at = qsizetype(key) + 0x594280;
    const auto extent = u32(rom.bytes(), at);
    if (extent > 16 * 1024 * 1024 || quint64(at) + 4 + extent > quint64(rom.bytes().size()))
        throw Error("Dialogue archive resource exceeds the ROM");
    // Parse inside the resource envelope, so extended headers cannot read a
    // neighboring allocation even when the overall ROM contains those bytes.
    auto member = LhaMember::read(rom.bytes().mid(at + 4, extent), 0);
    member.offset = at + 4;
    auto result = fromDecoded(member.decode(rom));
    result.name_ = member.name;
    return result;
}
DialogueArchive DialogueArchive::fromDecoded(QByteArray bytes) {
    const auto first = u32(bytes, 0);
    if (!first || first % 4 || first > quint32(bytes.size()) || first > 65536 * 4)
        throw Error("Dialogue entry directory has an invalid extent");
    DialogueArchive result;
    result.bytes_ = std::move(bytes);
    for (quint32 i = 0; i < first; i += 4) {
        const auto offset = u32(result.bytes_, i);
        if (offset < first || offset >= quint32(result.bytes_.size()) ||
            result.bytes_.indexOf('\0', offset) < 0)
            throw Error("Dialogue entry is outside its archive or lacks a terminator");
        result.offsets_ << offset;
    }
    return result;
}
QByteArray DialogueArchive::entry(int index) const {
    if (index < 0 || index >= offsets_.size())
        throw Error("Dialogue entry index exceeds its archive");
    const auto offset = offsets_[index];
    return bytes_.mid(offset, bytes_.indexOf('\0', offset) - offset);
}
} // namespace lordly
