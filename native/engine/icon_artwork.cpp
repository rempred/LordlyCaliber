#include "engine/icon_artwork.h"
#include "engine/resource.h"
#include <QJsonArray>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>
namespace lordly {
namespace {
constexpr quint32 resourceBase = 0x594280;
QColor color(quint16 word) {
    auto expand = [](int n) { return (n * 255 + 15) / 31; };
    return {expand(word >> 11), expand((word >> 6) & 31), expand((word >> 1) & 31),
            (word & 1) ? 255 : 0};
}
QByteArray planeWords(const QByteArray &data, int id) {
    QByteArray result(512, 0);
    for (int pixel = 0; pixel < 256; ++pixel)
        put16(result, pixel * 2, u16(data, u8(data, 512 + (id - 1) * 256 + pixel) * 2));
    return result;
}
QByteArray container(const QByteArray &decoded) {
    const auto encoded = encodeLzss(decoded);
    if (decodeLzss(encoded, decoded.size()) != decoded)
        throw Error("Icon resource compression failed readback");
    QByteArray result(8, 0);
    put32(result, 0, quint32(encoded.size() + 4));
    put32(result, 4, quint32(decoded.size()));
    return result + encoded;
}
QByteArray storedWords(const QJsonValue &value) {
    if (!value.isString())
        throw Error("Missing icon pixel words");
    const auto text = value.toString().toLatin1();
    const auto bytes = QByteArray::fromBase64(text, QByteArray::AbortOnBase64DecodingErrors);
    if (bytes.size() != 512 || bytes.toBase64() != text)
        throw Error("Icon needs exactly 256 RGBA5551 words");
    return bytes;
}
} // namespace
const std::array<IconPackSpec, 2> &IconArtwork::specs() {
    static const std::array<IconPackSpec, 2> data{{{"equipment",
                                                    "Equipment",
                                                    277,
                                                    255,
                                                    0x1dee82c,
                                                    {{0xaee94, 0xaee98},
                                                     {0x132bc0, 0x132bc8},
                                                     {0x19bb80, 0x19bb88},
                                                     {0x1b5e30, 0x1b5e38},
                                                     {0x2a53a4, 0x2a53ac},
                                                     {0x2a5448, 0x2a5454},
                                                     {0x2a6360, 0x2a636c},
                                                     {0x2a6640, 0x2a664c},
                                                     {0x2a67a0, 0x2a67ac}}},
                                                   {"special-item",
                                                    "Consumables",
                                                    44,
                                                    0,
                                                    0x1df7bf8,
                                                    {{0xaeeac, 0xaeeb0},
                                                     {0x102ac4, 0x102ad4},
                                                     {0x1ab730, 0x1ab73c},
                                                     {0x216aa4, 0x216aa8},
                                                     {0x2a53b0, 0x2a53b4},
                                                     {0x2a542c, 0x2a543c},
                                                     {0x2a6624, 0x2a6634},
                                                     {0x2a6784, 0x2a6794}}}}};
    return data;
}
int IconArtwork::packIndex(quint16 item) {
    const int index = (item & 0x200) ? 1 : 0, id = item & 0x1ff;
    if ((item & ~0x3ff) || id < 1 || id > specs()[index].count)
        throw Error("Icon ID is outside its native pack");
    return index;
}
QByteArray IconArtwork::canonicalOwner(QByteArray code, qsizetype start) {
    for (const auto &spec : specs())
        for (const auto &[hi, lo] : spec.sites) {
            if (hi < start || lo < start || hi + 4 > start + code.size() ||
                lo + 4 > start + code.size())
                continue;
            const auto high = u32(code, hi - start), low = u32(code, lo - start),
                       reg = (high >> 16) & 31;
            const auto key = ((high & 65535) << 16) | (low & 65535);
            if ((high & 0xffe00000U) != 0x3c000000U || !reg || (low >> 26) != 13 ||
                ((low >> 21) & 31) != reg || ((low >> 16) & 31) != reg || !key ||
                key >= 0x10000000 || key % 2)
                throw Error("Invalid icon operand in shared runtime owner");
            put32(code, hi - start, (high & 0xffff0000) | (spec.retailKey >> 16));
            put32(code, lo - start, (low & 0xffff0000) | (spec.retailKey & 65535));
        }
    return code;
}
void IconArtwork::validateRelocations(const Rom &rom) {
    if (rom.revision())
        return;
    for (const auto &spec : specs())
        for (const auto &[hi, lo] : spec.sites)
            if ((((u32(rom.bytes(), hi) & 65535) << 16) | (u32(rom.bytes(), lo) & 65535)) !=
                spec.retailKey) {
                load(rom);
                return; // Validate all links, both resources and any native ownership metadata.
            }
}
IconArtwork IconArtwork::load(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Icon artwork currently requires US revision zero");
    IconArtwork result;
    for (int index = 0; index < 2; ++index) {
        const auto &spec = specs()[index];
        auto &pack = result.packs_[index];
        for (const auto &[hi, lo] : spec.sites) {
            const auto high = u32(rom.bytes(), hi), low = u32(rom.bytes(), lo),
                       reg = (high >> 16) & 31;
            if ((high & 0xffe00000U) != 0x3c000000U || !reg || (low >> 26) != 13 ||
                ((low >> 21) & 31) != reg || ((low >> 16) & 31) != reg)
                throw Error("Unsupported " + spec.label + " icon resource owner");
            const auto key = ((high & 0xffff) << 16) | (low & 0xffff);
            if (!key || key >= 0x10000000 || key % 2 || (pack.key && pack.key != key))
                throw Error(spec.label + " icon resource owners disagree");
            pack.key = key;
            result.ownerWords_[hi] = high;
            result.ownerWords_[lo] = low;
        }
        const qsizetype at = resourceBase + pack.key;
        const auto stored = u32(rom.bytes(), at), decoded = u32(rom.bytes(), at + 4);
        if (stored < 4 || stored > 1024 * 1024 || decoded != quint32(512 + spec.count * 256))
            throw Error("Unsupported icon resource dimensions");
        pack.source = Resource::load(rom, at, qsizetype(stored) + 4).decoded;
        for (int i = 0; i < 256; ++i)
            if (bool(u16(pack.source, i * 2) & 1) == (i == spec.transparent))
                throw Error("Icon pack transparency index changed");
        for (int id = 1; id <= spec.count; ++id)
            result.originals_[quint16((index ? 0x200 : 0) | id)] = planeWords(pack.source, id);
    }
    const auto descriptor = rom.bytes().mid(descriptorOffset, descriptorSize);
    result.descriptor_ = descriptor;
    if (descriptor.startsWith("LC05ICON")) {
        if (descriptor.size() != descriptorSize || u32(descriptor, 8) != 1 ||
            descriptor.mid(36, 28) != QByteArray(28, 0))
            throw Error("Unsupported native icon descriptor");
        Placement placement;
        placement.offset = u32(descriptor, 12);
        placement.allocation = u32(descriptor, 16);
        if (placement.offset < 0x2800000 || placement.offset % 16 || !placement.allocation ||
            placement.allocation > 0x210000 || placement.allocation % 16 ||
            quint64(placement.offset) + placement.allocation > quint64(rom.bytes().size()))
            throw Error("Native icon allocation exceeds ROM bounds");
        for (int i = 0; i < 2; ++i) {
            placement.relative[i] = u32(descriptor, 20 + i * 4);
            placement.sizes[i] = u32(descriptor, 28 + i * 4);
            if (placement.relative[i] % 16 || placement.sizes[i] < 8 ||
                quint64(placement.relative[i]) + placement.sizes[i] > placement.allocation ||
                result.packs_[i].key != placement.offset + placement.relative[i] - resourceBase ||
                u32(rom.bytes(), placement.offset + placement.relative[i]) + 4 !=
                    placement.sizes[i])
                throw Error("Native icon resource link disagrees with its allocation");
        }
        if (placement.relative[0] != 0 || placement.relative[1] < placement.sizes[0])
            throw Error("Native icon resources overlap");
        if (placement.relative[1] != (placement.sizes[0] + 15U & ~15U) ||
            placement.allocation != ((placement.relative[1] + placement.sizes[1] + 15U) & ~15U))
            throw Error("Native icon allocation has unclaimed spans");
        for (int i = 0; i < 2; ++i) {
            const quint32 end = placement.relative[i] + placement.sizes[i],
                          next = i == 0 ? placement.relative[1] : placement.allocation;
            if (rom.bytes().mid(placement.offset + end, next - end) !=
                QByteArray(next - end, char(-1)))
                throw Error("Native icon padding was modified");
        }
        placement.hash = descriptor.mid(64, 32);
        if (QByteArray::fromHex(sha256(rom.bytes().mid(placement.offset, placement.allocation))) !=
            placement.hash)
            throw Error("Native icon allocation contains unrecognized changes");
        result.placement_ = placement;
    }
    return result;
}
const QByteArray &IconArtwork::words(quint16 item) const {
    packIndex(item);
    const auto edit = edits_.constFind(item);
    if (edit != edits_.cend())
        return edit.value();
    const auto original = originals_.constFind(item);
    if (original == originals_.cend())
        throw Error("Icon artwork has not been loaded");
    return original.value();
}
QImage IconArtwork::image(quint16 item) const {
    const auto &data = words(item);
    QImage image(16, 16, QImage::Format_RGBA8888);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            image.setPixelColor(x, y, color(u16(data, (y * 16 + x) * 2)));
    return image;
}
QVector<quint16> IconArtwork::palette(int index) const {
    if (index < 0 || index > 1)
        throw Error("Invalid icon pack");
    QVector<quint16> result;
    const auto decoded = decodedPacks()[index];
    for (int i = 0; i < 256; ++i) {
        const auto word = u16(decoded, i * 2);
        if (word & 1)
            result.append(word);
    }
    return result;
}
QByteArray IconArtwork::mapImage(quint16 item, const QImage &input, bool existing,
                                 int threshold) const {
    const int index = packIndex(item);
    if (input.size() != QSize(16, 16) || threshold < 1 || threshold > 255)
        throw Error("Icon assignment needs a 16 × 16 image and alpha threshold 1–255");
    const auto image = input.convertToFormat(QImage::Format_RGBA8888);
    const auto colors = existing ? palette(index) : QVector<quint16>{};
    QByteArray result(512, 0);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            const auto pixel = image.pixelColor(x, y);
            quint16 word = u16(packs_[index].source, specs()[index].transparent * 2);
            if (pixel.alpha() >= threshold) {
                word = quint16((((pixel.red() * 31 + 127) / 255) << 11) |
                               (((pixel.green() * 31 + 127) / 255) << 6) |
                               (((pixel.blue() * 31 + 127) / 255) << 1) | 1);
                if (existing) {
                    int best = std::numeric_limits<int>::max();
                    for (const auto candidate : colors) {
                        const auto c = color(candidate);
                        const int r = c.red() - pixel.red(), g = c.green() - pixel.green(),
                                  b = c.blue() - pixel.blue(),
                                  distance = 77 * r * r + 150 * g * g + 29 * b * b;
                        if (distance < best) {
                            word = candidate;
                            best = distance;
                        }
                    }
                }
            }
            put16(result, (y * 16 + x) * 2, word);
        }
    return result;
}
void IconArtwork::setWords(quint16 item, const QByteArray &bytes) {
    const int index = packIndex(item);
    if (bytes.size() != 512)
        throw Error("Icon needs 256 pixel words");
    const auto transparent = u16(packs_[index].source, specs()[index].transparent * 2);
    for (int i = 0; i < 256; ++i)
        if (!(u16(bytes, i * 2) & 1) && u16(bytes, i * 2) != transparent)
            throw Error("Transparent icon pixels must use their pack's transparent word");
    auto candidate = *this;
    if (bytes == originals_[item])
        candidate.edits_.remove(item);
    else
        candidate.edits_[item] = bytes;
    candidate.decodedPacks();
    *this = std::move(candidate);
}
void IconArtwork::restore(quint16 item) {
    packIndex(item);
    edits_.remove(item);
}
std::array<QByteArray, 2> IconArtwork::decodedPacks() const {
    std::array<QByteArray, 2> result;
    for (int index = 0; index < 2; ++index) {
        const auto &spec = specs()[index];
        QVector<quint16> required;
        QSet<quint16> seen;
        for (int id = 1; id <= spec.count; ++id) {
            const auto &data = words(quint16((index ? 0x200 : 0) | id));
            for (int pixel = 0; pixel < 256; ++pixel) {
                const auto word = u16(data, pixel * 2);
                if ((word & 1) && !seen.contains(word)) {
                    seen.insert(word);
                    required.append(word);
                }
            }
        }
        if (required.size() > 255)
            throw Error(spec.label +
                        " icons use more than 255 opaque colors across the shared pack");
        auto palette = packs_[index].source.first(512);
        QMap<quint16, int> assigned;
        QSet<int> used{spec.transparent};
        for (const auto word : required)
            for (int i = 0; i < 256; ++i)
                if (!used.contains(i) && u16(palette, i * 2) == word) {
                    assigned[word] = i;
                    used.insert(i);
                    break;
                }
        int free = 0;
        for (const auto word : required)
            if (!assigned.contains(word)) {
                while (used.contains(free))
                    ++free;
                assigned[word] = free;
                put16(palette, free * 2, word);
                used.insert(free);
            }
        auto out = palette;
        for (int id = 1; id <= spec.count; ++id) {
            const auto &data = words(quint16((index ? 0x200 : 0) | id));
            for (int pixel = 0; pixel < 256; ++pixel) {
                const auto word = u16(data, pixel * 2);
                out.append(char(word & 1 ? assigned[word] : spec.transparent));
            }
        }
        result[index] = std::move(out);
    }
    return result;
}
QJsonObject IconArtwork::json() const {
    QJsonObject edits;
    for (auto it = edits_.cbegin(); it != edits_.cend(); ++it)
        edits[QString::number(it.key())] = QString::fromLatin1(it.value().toBase64());
    return {{"version", 1}, {"icons", edits}};
}
void IconArtwork::apply(const QJsonObject &object) {
    if (object["version"] != 1 || !object["icons"].isObject())
        throw Error("Unsupported icon artwork project");
    auto candidate = *this;
    candidate.edits_.clear();
    const auto icons = object["icons"].toObject();
    // Validate complete packs atomically: a multi-icon edit may replace shared colors.
    for (auto it = icons.begin(); it != icons.end(); ++it) {
        bool ok = false;
        const auto id = it.key().toUInt(&ok);
        if (!ok || id > 0xffff || QString::number(id) != it.key())
            throw Error("Invalid icon ID");
        packIndex(quint16(id));
        candidate.edits_[quint16(id)] = storedWords(it.value());
    }
    for (auto it = candidate.edits_.cbegin(); it != candidate.edits_.cend(); ++it) {
        const auto index = packIndex(it.key());
        const auto transparent = u16(packs_[index].source, specs()[index].transparent * 2);
        for (int i = 0; i < 256; ++i)
            if (!(u16(it.value(), i * 2) & 1) && u16(it.value(), i * 2) != transparent)
                throw Error("Invalid transparent icon word");
    }
    candidate.decodedPacks();
    for (auto it = candidate.edits_.begin(); it != candidate.edits_.end();)
        if (it.value() == candidate.originals_[it.key()])
            it = candidate.edits_.erase(it);
        else
            ++it;
    *this = std::move(candidate);
}
void IconArtwork::importLegacy(const QJsonObject &object) {
    const auto version = object["schemaVersion"].toDouble();
    if (version < 1 || version > 5 || version != std::floor(version))
        throw Error("Unsupported imported art schema");
    if (!object["icons"].isUndefined() && !object["icons"].isObject())
        throw Error("Invalid imported icons");
    auto settings = json();
    auto edits = settings["icons"].toObject();
    const auto icons = object["icons"].toObject();
    for (auto it = icons.begin(); it != icons.end(); ++it) {
        const auto row = it.value().toObject();
        int index = -1;
        for (int i = 0; i < 2; ++i)
            if (row["pack"] == specs()[i].slug)
                index = i;
        const double id = row["itemId"].toDouble(-1);
        if (index < 0 || id < 1 || id > specs()[index].count || id != std::floor(id) ||
            it.key() != specs()[index].slug + ":" + QString::number(int(id)))
            throw Error("Imported icon identity is invalid");
        edits[QString::number((index ? 0x200 : 0) | int(id))] = row["pixelsRgba5551BeBase64"];
    }
    settings["icons"] = edits;
    apply(settings);
}
QByteArray IconArtwork::rebuild(const Rom &rom, const std::array<QByteArray, 2> &compiled) const {
    if (compiled != decodedPacks())
        throw Error("Compiled icon pixels differ from the project");
    if (!modified())
        return rom.bytes();
    if (rom.bytes().mid(descriptorOffset, descriptorSize) != descriptor_)
        throw Error("Another export owner changed the icon descriptor");
    for (auto it = ownerWords_.cbegin(); it != ownerWords_.cend(); ++it)
        if (u32(rom.bytes(), it.key()) != it.value())
            throw Error("Another export owner changed an icon resource operand");
    if (placement_) {
        if (QByteArray::fromHex(sha256(
                rom.bytes().mid(placement_->offset, placement_->allocation))) != placement_->hash)
            throw Error("Another export owner changed the icon allocation");
    } else if (rom.bytes().mid(descriptorOffset, descriptorSize) !=
               QByteArray(descriptorSize, char(-1)))
        throw Error("Native icon descriptor allocation is occupied");
    const std::array<QByteArray, 2> resources{container(compiled[0]), container(compiled[1])};
    auto payload = resources[0];
    payload += QByteArray((16 - payload.size() % 16) % 16, char(-1));
    const auto second = quint32(payload.size());
    payload += resources[1];
    payload += QByteArray((16 - payload.size() % 16) % 16, char(-1));
    const auto allocation = quint32(payload.size());
    const bool tail = placement_ && quint64(placement_->offset) + placement_->allocation ==
                                        quint64(rom.bytes().size());
    const quint32 offset = placement_ && (allocation <= placement_->allocation || tail)
                               ? placement_->offset
                               : quint32((rom.bytes().size() + 15) & ~qsizetype(15));
    if (offset < 0x2800000 || quint64(offset) + allocation > 0x4000000)
        throw Error("Rebuilt icons exceed the cartridge capacity");
    auto image = rom.bytes();
    if (placement_)
        image.replace(placement_->offset, placement_->allocation,
                      QByteArray(placement_->allocation, char(-1)));
    if (tail && offset == placement_->offset)
        image.resize(offset);
    if (quint64(image.size()) < quint64(offset) + allocation)
        image += QByteArray(offset + allocation - image.size(), char(-1));
    image.replace(offset, allocation, payload);
    for (int index = 0; index < 2; ++index) {
        const auto key = offset + (index ? second : 0) - resourceBase;
        for (const auto &[hi, lo] : specs()[index].sites) {
            put32(image, hi, (u32(image, hi) & 0xffff0000) | (key >> 16));
            put32(image, lo, (u32(image, lo) & 0xffff0000) | (key & 0xffff));
        }
    }
    QByteArray descriptor(descriptorSize, 0);
    descriptor.replace(0, 8, "LC05ICON");
    put32(descriptor, 8, 1);
    put32(descriptor, 12, offset);
    put32(descriptor, 16, allocation);
    put32(descriptor, 24, second);
    put32(descriptor, 28, quint32(resources[0].size()));
    put32(descriptor, 32, quint32(resources[1].size()));
    descriptor.replace(64, 32, QByteArray::fromHex(sha256(payload)));
    image.replace(descriptorOffset, descriptorSize, descriptor);
    const auto reopened = load(Rom::fromBytes(image));
    for (auto it = originals_.cbegin(); it != originals_.cend(); ++it)
        if (reopened.words(it.key()) != words(it.key()))
            throw Error("Rebuilt icon readback differs from the project");
    return image;
}
} // namespace lordly
