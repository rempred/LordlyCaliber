#include "engine/direct_descriptors.h"
namespace lordly {
namespace {
quint32 keyWords(quint32 high, quint32 low) {
    const auto reg = (high >> 16) & 31;
    if ((high & 0xffe00000U) != 0x3c000000U || (reg != 4 && reg != 5) || low >> 26 != 13 ||
        ((low >> 21) & 31) != reg || ((low >> 16) & 31) != reg)
        throw Error("Unrecognized direct-descriptor resource operand");
    const auto key = ((high & 65535) << 16) | (low & 65535);
    if (!key || key % 2 || key >= 0x4000000 - DirectDescriptors::resourceBase)
        throw Error("Direct-descriptor resource key is outside the cartridge");
    return key;
}
void validateKey(quint32 key) {
    if (key && (key % 2 || key >= 0x4000000 - DirectDescriptors::resourceBase))
        throw Error("Direct descriptor key is outside the cartridge");
}
void references(const QByteArray &image, const QVector<quint32> &keys) {
    for (auto key : keys) {
        validateKey(key);
        if (!key)
            continue;
        const auto at = quint64(key) + DirectDescriptors::resourceBase;
        if (at + 4 > quint64(image.size()))
            throw Error("Direct descriptor reference exceeds the ROM");
        const auto size = u32(image, qsizetype(at));
        if (size < 16 || size % 4 || size > 65540 * 4 || at + 4 + size > quint64(image.size()))
            throw Error("Direct descriptor reference has invalid bounds");
    }
}
} // namespace
const std::array<QPair<int, int>, 12> &DirectDescriptors::sites() {
    static const std::array<QPair<int, int>, 12> value{{{0x6fee4, 0x6feec},
                                                        {0x6ff00, 0x6ff08},
                                                        {0x1b8aac, 0x1b8ab4},
                                                        {0x1b8ac8, 0x1b8ad0},
                                                        {0x1d58a8, 0x1d58ac},
                                                        {0x1d58dc, 0x1d58e4},
                                                        {0x1e3694, 0x1e3698},
                                                        {0x1e36b4, 0x1e36bc},
                                                        {0x205d5c, 0x205d64},
                                                        {0x205d78, 0x205d80},
                                                        {0x207444, 0x207448},
                                                        {0x207464, 0x20746c}}};
    return value;
}
quint32 DirectDescriptors::resourceKey(const Rom &rom) {
    quint32 key = 0;
    for (const auto &[hi, lo] : sites()) {
        const auto next = keyWords(u32(rom.bytes(), hi), u32(rom.bytes(), lo));
        if (key && next != key)
            throw Error("Direct-descriptor resource users disagree");
        key = next;
    }
    return key;
}
QByteArray DirectDescriptors::directory(const Rom &rom) {
    return load(rom).decoded();
}
DirectDescriptors DirectDescriptors::load(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Direct descriptors currently require US revision zero");
    DirectDescriptors result;
    result.source_ = rom.bytes();
    result.descriptor_ = rom.bytes().mid(descriptorOffset, descriptorSize);
    const auto key = resourceKey(rom), at = key + resourceBase, size = u32(rom.bytes(), at);
    if (!size || size % 4 || size > 4095 * 4 ||
        quint64(at) + 4 + size > quint64(rom.bytes().size()))
        throw Error("Direct descriptor directory has invalid bounds");
    for (quint32 pos = 0; pos < size; pos += 4)
        result.keys_ << u32(rom.bytes(), at + 4 + pos);
    references(rom.bytes(), result.keys_);
    for (const auto &[hi, lo] : sites()) {
        result.words_[hi] = u32(rom.bytes(), hi);
        result.words_[lo] = u32(rom.bytes(), lo);
    }
    const auto &d = result.descriptor_;
    if (d.left(8) != "LC05DRTE") {
        if (key != retailKey)
            throw Error("Relocated direct descriptors have no authenticated owner");
        return result;
    }
    if (d.size() != descriptorSize || u32(d, 8) != 1)
        throw Error("Invalid direct-descriptor allocation header");
    result.offset_ = u32(d, 12);
    result.size_ = u32(d, 16);
    if (result.offset_ < 0x2800000 || result.offset_ % 16 || !result.size_ || result.size_ % 16 ||
        result.size_ > 16400 ||
        quint64(result.offset_) + result.size_ > quint64(rom.bytes().size()) ||
        result.offset_ != at || size + 4 > result.size_ || u32(d, 20) != size / 4 ||
        d.mid(24, 40) != QByteArray(40, 0))
        throw Error("Direct-descriptor allocation has invalid bounds or references");
    const auto payload = rom.bytes().mid(result.offset_, result.size_);
    if (QByteArray::fromHex(sha256(payload)) != d.mid(64, 32) ||
        payload.mid(size + 4) != QByteArray(payload.size() - size - 4, char(-1)))
        throw Error("Direct-descriptor allocation differs from its stored identity");
    return result;
}
QByteArray DirectDescriptors::canonicalOwner(QByteArray bytes, qsizetype start) {
    for (const auto &[hi, lo] : sites()) {
        if (hi < start || lo < start || hi + 4 > start + bytes.size() ||
            lo + 4 > start + bytes.size())
            continue;
        const auto high = u32(bytes, hi - start), low = u32(bytes, lo - start);
        keyWords(high, low);
        put32(bytes, hi - start, (high & 0xffff0000U) | (retailKey >> 16));
        put32(bytes, lo - start, (low & 0xffff0000U) | (retailKey & 65535));
    }
    return bytes;
}
void DirectDescriptors::validateRelocations(const Rom &rom) {
    if (!rom.revision() &&
        (rom.bytes().mid(descriptorOffset, 8) == "LC05DRTE" || resourceKey(rom) != retailKey))
        load(rom);
}
int DirectDescriptors::append(quint32 key) {
    validateKey(key);
    if (!key || keys_.size() >= 4095)
        throw Error("Direct descriptor table exceeds the twelve-bit handle domain");
    keys_ << key;
    return int(keys_.size());
}
void DirectDescriptors::set(int id, quint32 key) {
    validateKey(key);
    if (id < 1 || id > keys_.size())
        throw Error("Direct descriptor handle is absent");
    keys_[id - 1] = key;
}
QByteArray DirectDescriptors::decoded() const {
    QByteArray bytes(keys_.size() * 4, 0);
    for (int i = 0; i < keys_.size(); ++i)
        put32(bytes, i * 4, keys_[i]);
    return bytes;
}
QByteArray DirectDescriptors::rebuild(const Rom &rom, const QByteArray &compiled) const {
    if (compiled != decoded())
        throw Error("Compiled descriptor directory differs from the project");
    references(rom.bytes(), keys_);
    if (rom.bytes().mid(descriptorOffset, descriptorSize) != descriptor_ ||
        (!offset_ && descriptor_ != QByteArray(descriptorSize, char(-1))))
        throw Error("Direct descriptor allocation is occupied or changed by another owner");
    for (auto it = words_.cbegin(); it != words_.cend(); ++it)
        if (u32(rom.bytes(), it.key()) != it.value())
            throw Error("Another export owner changed a direct-descriptor operand");
    if (offset_ && rom.bytes().mid(offset_, size_) != source_.mid(offset_, size_))
        throw Error("Another export owner changed the direct-descriptor allocation");
    const auto oldKey = resourceKey(rom), oldAt = oldKey + resourceBase;
    const auto originalSize = u32(source_, oldAt);
    if (rom.bytes().mid(oldAt, 4 + originalSize) != source_.mid(oldAt, 4 + originalSize))
        throw Error("Another export owner changed the direct-descriptor directory");
    if (u32(rom.bytes(), oldAt) == quint32(compiled.size()) &&
        rom.bytes().mid(oldAt + 4, compiled.size()) == compiled)
        return rom.bytes();
    QByteArray payload(4, 0);
    put32(payload, 0, quint32(compiled.size()));
    payload += compiled;
    payload += QByteArray((16 - payload.size() % 16) % 16, char(-1));
    const bool tail = offset_ && quint64(offset_) + size_ == quint64(rom.bytes().size());
    const quint32 offset = offset_ && (payload.size() <= size_ || tail)
                               ? offset_
                               : (quint32(rom.bytes().size()) + 15) & ~15U;
    if (offset < 0x2800000 || quint64(offset) + payload.size() > 0x4000000)
        throw Error("Direct descriptors exceed cartridge capacity");
    auto image = rom.bytes();
    if (offset_)
        image.replace(offset_, size_, QByteArray(size_, char(-1)));
    if (tail && offset == offset_)
        image.resize(offset);
    if (quint64(image.size()) < quint64(offset) + payload.size())
        image += QByteArray(offset + payload.size() - image.size(), char(-1));
    image.replace(offset, payload.size(), payload);
    const auto key = offset - resourceBase;
    for (const auto &[hi, lo] : sites()) {
        put32(image, hi, (u32(image, hi) & 0xffff0000U) | (key >> 16));
        put32(image, lo, (u32(image, lo) & 0xffff0000U) | (key & 65535));
    }
    QByteArray d(descriptorSize, 0);
    d.replace(0, 8, "LC05DRTE");
    put32(d, 8, 1);
    put32(d, 12, offset);
    put32(d, 16, quint32(payload.size()));
    put32(d, 20, quint32(keys_.size()));
    d.replace(64, 32, QByteArray::fromHex(sha256(payload)));
    image.replace(descriptorOffset, descriptorSize, d);
    if (load(Rom::fromBytes(image)).decoded() != compiled)
        throw Error("Direct descriptors differ after readback");
    return image;
}
} // namespace lordly
