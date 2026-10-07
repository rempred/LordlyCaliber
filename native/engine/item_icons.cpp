#include "engine/item_icons.h"
#include "engine/resource.h"
namespace lordly {
const QByteArray &ItemIcons::pack(bool consumable) const {
    auto &result = packs_[consumable ? 1 : 0];
    if (!result.isEmpty())
        return result;
    rom_.requireSupported();
    if (rom_.revision())
        throw Error("Item icons currently require US revision zero");
    // Follow the reward constructor's current resource operands, including
    // relocated packs. These are LUI a0 / ORI a0,a0 pairs, not RAM addresses.
    const auto high = u32(rom_.bytes(), consumable ? 0x2a6624 : 0x2a6640);
    const auto low = u32(rom_.bytes(), consumable ? 0x2a6634 : 0x2a664c);
    if ((high >> 16) != 0x3c04 || (low >> 16) != 0x3484)
        throw Error("Item icon resource operands use an unsupported instruction layout");
    const quint32 key = ((high & 0xffffU) << 16) | (low & 0xffffU);
    if (!key || key >= 0x10000000 || (key & 1))
        throw Error("Invalid item icon resource key");
    const qsizetype at = 0x594280 + key;
    const quint32 stored = u32(rom_.bytes(), at), decoded = u32(rom_.bytes(), at + 4);
    const int expected = 512 + (consumable ? 44 : 277) * 256;
    if (stored > 1024 * 1024 || decoded < quint32(expected) || (decoded - 512) % 256 ||
        decoded > quint32(512 + (consumable ? 44 : 510) * 256))
        throw Error("Item icon pack has an unsupported size");
    result = Resource::load(rom_, at, qsizetype(stored) + 4).decoded;
    return result;
}
RasterImage ItemIcons::image(quint16 item) const {
    const bool consumable = item & 0x200;
    const int index = item & 0x1ff;
    if ((item & ~0x3ff) || index > (consumable ? 44 : 510))
        throw Error("Item icon is outside its native pack");
    if (!index)
        return {16, 16, QByteArray(1024, 0)};
    const auto &source = pack(consumable);
    if (512 + index * 256 > source.size())
        throw Error("Item icon exceeds the loaded pack");
    // Reuse the standard bounded CI8 image decoder; item packs store their
    // shared palette first, while native image containers store it last.
    QByteArray image = QByteArray::fromHex("3634020100100010");
    image += source.mid(512 + (index - 1) * 256, 256);
    image += source.first(512);
    return decodeN64Image(image);
}
} // namespace lordly
