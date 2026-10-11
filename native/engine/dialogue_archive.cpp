#include "engine/dialogue_archive.h"
namespace lordly {
QVector<quint32> DialogueArchive::directory(const Rom &rom, Table table) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Dialogue archives currently require US revision zero");
    const qsizetype at = (table == Table::Primary ? 0x1a3b7b2 : 0x21b8ba4) + 0x594280;
    const auto size = u32(rom.bytes(), at);
    if (!size || size % 4 || size > 65536 || quint64(at) + 4 + size > quint64(rom.bytes().size()))
        throw Error("Dialogue archive directory exceeds its ROM allocation");
    QVector<quint32> keys;
    for (quint32 i = 0; i < size; i += 4)
        keys << u32(rom.bytes(), at + 4 + i);
    return keys;
}
DialogueArchive DialogueArchive::load(const Rom &rom, int selector, Table table) {
    const auto keys = directory(rom, table);
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
QByteArray DialogueArchive::windowSource(const Rom &rom, const QByteArray &window) {
    if (window.size() != 168)
        throw Error("Dialogue source lookup requires a complete window record");
    const auto kind = u8(window, 0x88) & 0xc0;
    if (kind != 0 && kind != 0x80)
        throw Error("Dialogue window selects an unresolved archive table group");
    const auto table = kind == 0 ? Table::Primary : Table::Alternate;
    return load(rom, u16(window, 0x96), table).entry(u16(window, 0x88) & 0x3fff) + char(0);
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
QByteArray DialogueArchive::rebuild(const QMap<int, QByteArray> &edits,
                                    const QVector<QByteArray> &added) const {
    const qsizetype count = offsets_.size() + added.size();
    if (count > 16384)
        throw Error("Dialogue entries exceed the native fourteen-bit index");
    auto validate = [](const QByteArray &text) {
        if (text.contains(char(0)))
            throw Error("Dialogue text contains an embedded terminator");
        if (text.size() > 1024 * 1024)
            throw Error("Dialogue entry exceeds the source budget");
    };
    bool changed = !added.isEmpty();
    for (auto it = edits.cbegin(); it != edits.cend(); ++it) {
        validate(it.value());
        changed |= entry(it.key()) != it.value(); // Also validates the original index.
    }
    for (const auto &text : added)
        validate(text);
    if (!changed)
        return bytes_;
    // Entry zero's offset also encodes the directory extent. If it changes,
    // place its new string directly after the expanded directory; retain the old
    // payload after it so unchanged aliases still point to their original text.
    const bool firstChanged = edits.contains(0) && edits[0] != entry(0);
    const QByteArray first = firstChanged ? edits[0] + char(0) : QByteArray();
    const quint32 delta = quint32(added.size() * 4 + first.size());
    QByteArray result(count * 4, 0);
    result += first;
    result += bytes_.mid(offsets_.size() * 4);
    if (result.size() > 16 * 1024 * 1024)
        throw Error("Rebuilt dialogue archive exceeds its resource budget");
    for (int i = 0; i < offsets_.size(); ++i)
        put32(result, i * 4, offsets_[i] + delta);
    QMap<QByteArray, quint32> appended;
    if (firstChanged) {
        put32(result, 0, quint32(count * 4));
        appended[edits[0]] = quint32(count * 4);
    }
    auto append = [&](int index, const QByteArray &text) {
        if (!appended.contains(text)) {
            if (result.size() + text.size() + 1 > 16 * 1024 * 1024)
                throw Error("Rebuilt dialogue archive exceeds its resource budget");
            appended[text] = quint32(result.size());
            result += text;
            result += char(0);
        }
        put32(result, index * 4, appended[text]);
    };
    for (auto it = edits.cbegin(); it != edits.cend(); ++it)
        if (entry(it.key()) != it.value())
            append(it.key(), it.value());
    for (int i = 0; i < added.size(); ++i)
        append(int(offsets_.size()) + i, added[i]);
    // Reuse the reader's bounds and termination validation on the whole result.
    fromDecoded(result);
    return result;
}
} // namespace lordly
