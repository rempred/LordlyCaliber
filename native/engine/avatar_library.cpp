#include "engine/avatar_library.h"
#include "engine/resource.h"
namespace lordly {
const QVector<QPair<int, int>> &AvatarLibrary::sites() {
    static const QVector<QPair<int, int>> result{
        {0x45afc, 0x45b00}, {0x45b6c, 0x45b70}, {0x45c58, 0x45c5c}};
    return result;
}
namespace {
quint32 keyFrom(quint32 high, quint32 low) {
    const auto reg = (high >> 16) & 31, key = ((high & 65535) << 16) | (low & 65535);
    if ((high & 0xffe00000U) != 0x3c000000U || !reg || (low >> 26) != 13 ||
        ((low >> 21) & 31) != reg || ((low >> 16) & 31) != reg || !key || key >= 0x10000000 ||
        key % 2)
        throw Error("Unrecognized avatar directory resource operand");
    return key;
}
} // namespace
QByteArray AvatarLibrary::canonicalOwner(QByteArray code, qsizetype start) {
    for (const auto &[hi, lo] : sites()) {
        if (hi < start || lo < start || hi + 4 > start + code.size() ||
            lo + 4 > start + code.size())
            continue;
        const auto high = u32(code, hi - start), low = u32(code, lo - start);
        keyFrom(high, low);
        put32(code, hi - start, (high & 0xffff0000) | (retailKey >> 16));
        put32(code, lo - start, (low & 0xffff0000) | (retailKey & 65535));
    }
    return code;
}
void AvatarLibrary::validateRelocations(const Rom &rom) {
    if (rom.revision())
        return;
    for (const auto &[hi, lo] : sites())
        if ((((u32(rom.bytes(), hi) & 65535) << 16) | (u32(rom.bytes(), lo) & 65535)) !=
            retailKey) {
            load(rom);
            return;
        }
}
AvatarLibrary AvatarLibrary::load(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Avatars currently require US revision zero");
    AvatarLibrary result;
    result.rom_ = rom;
    for (const auto &[hi, lo] : sites()) {
        const auto key = keyFrom(u32(rom.bytes(), hi), u32(rom.bytes(), lo));
        if (result.key_ && result.key_ != key)
            throw Error("Avatar directory owners disagree");
        result.key_ = key;
    }
    const auto at = result.key_ + resourceBase, size = u32(rom.bytes(), at);
    if (size < 4 || size % 4 || size > 8192 * 4 ||
        quint64(at) + 4 + size > quint64(rom.bytes().size()))
        throw Error("Avatar directory exceeds the packed selector domain");
    for (quint32 i = 0; i < size / 4; ++i) {
        const auto key = u32(rom.bytes(), at + 4 + i * 4);
        if ((i == 0 && key != 0) ||
            (i > 0 && (!key || key % 2 || key >= 0x10000000 ||
                       quint64(resourceBase) + key + 8 > quint64(rom.bytes().size()))))
            throw Error("Invalid avatar bundle resource key");
        result.members_.append(key);
    }
    return result;
}
const AvatarBundle &AvatarLibrary::bundle(int member) const {
    if (member < 1 || member >= members_.size())
        throw Error("Avatar member index is outside the current directory");
    const auto key = members_[member];
    if (!bundles_.contains(key)) {
        const auto at = resourceBase + key, stored = u32(rom_.bytes(), at),
                   decoded = u32(rom_.bytes(), at + 4);
        if (stored < 4 || stored > 8192 || decoded < 2080 || decoded > 3200 ||
            (decoded - 1920) % 160)
            throw Error("Avatar bundle dimensions are unsupported");
        const auto data = Resource::load(rom_, at, qsizetype(stored) + 4).decoded;
        AvatarBundle bundle;
        bundle.indices = data.first(1920);
        for (const auto index : bundle.indices)
            if (quint8(index) >= 80)
                throw Error("Avatar pixel index exceeds its 80-color palette");
        for (qsizetype pos = 1920; pos < data.size(); pos += 160) {
            const auto palette = data.mid(pos, 160);
            for (int i = 0; i < 80; ++i)
                if (!(u16(palette, i * 2) & 1))
                    throw Error("Avatar palette contains unsupported transparent colors");
            bundle.palettes.append(palette);
        }
        bundles_.insert(key, std::move(bundle));
    }
    return bundles_.constFind(key).value();
}
QByteArray AvatarLibrary::palette(quint16 token) const {
    if (!token)
        return {};
    const auto &source = bundle(token >> 3);
    if ((token & 7) >= source.palettes.size())
        throw Error("Avatar palette ordinal exceeds its bundle");
    return source.palettes[token & 7];
}
QByteArray AvatarLibrary::words(quint16 token) const {
    if (!token)
        return {};
    const auto colors = palette(token);
    const auto &source = bundle(token >> 3);
    QByteArray result(3840, 0);
    for (int i = 0; i < 1920; ++i)
        put16(result, i * 2, u16(colors, u8(source.indices, i) * 2));
    return result;
}
QImage AvatarLibrary::image(quint16 token) const {
    QImage image(40, 48, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    if (!token)
        return image;
    const auto data = words(token);
    auto expand = [](int n) { return (n * 255 + 15) / 31; };
    for (int y = 0; y < 48; ++y)
        for (int x = 0; x < 40; ++x) {
            const auto word = u16(data, (y * 40 + x) * 2);
            image.setPixelColor(
                x, y,
                {expand(word >> 11), expand((word >> 6) & 31), expand((word >> 1) & 31), 255});
        }
    return image;
}
} // namespace lordly
