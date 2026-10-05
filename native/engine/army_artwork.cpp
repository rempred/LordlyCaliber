#include "engine/army_artwork.h"
#include "engine/resource.h"
#include <algorithm>
#include <limits>
namespace lordly {
const std::array<ArmyAtlasSpec, 6> &ArmyArtwork::specs() {
    static const std::array<ArmyAtlasSpec, 6> result{
        {{"enemy-front-ordinary",
          "Front / Army board · Ordinary",
          16,
          24,
          56,
          56,
          0,
          0x1dbb2ac,
          {0},
          {{0x83800, 0x83810},
           {0xaef0c, 0xaef10},
           {0x135834, 0x135844},
           {0x194f20, 0x194f2c},
           {0x2093dc, 0x2093e4},
           {0x20954c, 0x209550},
           {0x27fc38, 0x27fc48}}},
         {"player-back-ordinary",
          "Player back · Ordinary",
          16,
          24,
          56,
          56,
          0x200,
          0x1dbef10,
          {70},
          {{0x2093dc, 0x2093ec}, {0x2095a8, 0x2095ac}}},
         {"enemy-front-special",
          "Front / Army board · Special",
          16,
          24,
          38,
          38,
          0,
          0x1dc3320,
          {255},
          {{0x83834, 0x83838},
           {0xaef24, 0xaef28},
           {0x13586c, 0x135870},
           {0x194f30, 0x194f40},
           {0x20942c, 0x209434},
           {0x20960c, 0x209610},
           {0x27fc60, 0x27fc70}}},
         {"player-back-special",
          "Player back · Special",
          16,
          24,
          24,
          38,
          0,
          0x1dc5e6c,
          {255},
          {{0x20942c, 0x209438}, {0x209668, 0x20966c}}},
         {"enemy-front-large",
          "Front / Army board · Large",
          32,
          28,
          26,
          26,
          0,
          0x1db47a0,
          {0},
          {{0x8381c, 0x83820},
           {0xaeef4, 0xaeef8},
           {0x135848, 0x135854},
           {0x2093f4, 0x2093fc},
           {0x2096cc, 0x2096d0},
           {0x27fc4c, 0x27fc5c}}},
         {"player-back-large",
          "Player back · Large",
          32,
          28,
          25,
          26,
          0,
          0x1db7f5c,
          {0, 177, 178, 179, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255},
          {{0x2093f4, 0x209400}, {0x209728, 0x20972c}}}}};
    return result;
}
namespace {
quint32 resourceKey(quint32 high, quint32 low) {
    const auto reg = (high >> 16) & 31;
    const auto key = ((high & 65535) << 16) | (low & 65535);
    if ((high & 0xffe00000U) != 0x3c000000U || reg != 4 || low >> 26 != 13 ||
        ((low >> 21) & 31) != reg || ((low >> 16) & 31) != reg || !key || key % 2 ||
        key >= 0x10000000)
        throw Error("Unrecognized Army sprite resource operand");
    return key;
}
QColor color(quint16 word) {
    return {(255 * (word >> 11) + 15) / 31, (255 * ((word >> 6) & 31) + 15) / 31,
            (255 * ((word >> 1) & 31) + 15) / 31, (word & 1) ? 255 : 0};
}
QByteArray decodeBase64(const QJsonValue &value, int size) {
    const auto text = value.toString().toLatin1(),
               bytes = QByteArray::fromBase64(text, QByteArray::AbortOnBase64DecodingErrors);
    if (!value.isString() || bytes.size() != size || bytes.toBase64() != text)
        throw Error("Invalid Army sprite pixel data");
    return bytes;
}
} // namespace
const ArmyAtlasSpec &ArmyArtwork::validate(int atlas) {
    if (atlas < 0 || atlas >= 6)
        throw Error("Army atlas is outside the supported set");
    return specs()[atlas];
}
const ArmyAtlasSpec &ArmyArtwork::validate(int atlas, int model) {
    const auto &spec = validate(atlas);
    if (model < 0 || model >= spec.capacity)
        throw Error("Army sprite is outside the supported atlas");
    return spec;
}
QByteArray ArmyArtwork::canonicalOwner(QByteArray code, qsizetype start) {
    for (const auto &spec : specs())
        for (const auto &[hi, lo] : spec.sites) {
            if (hi < start || lo < start || hi + 4 > start + code.size() ||
                lo + 4 > start + code.size())
                continue;
            const auto high = u32(code, hi - start), low = u32(code, lo - start);
            resourceKey(high, low);
            put32(code, hi - start, (high & 0xffff0000) | (spec.retailKey >> 16));
            put32(code, lo - start, (low & 0xffff0000) | (spec.retailKey & 65535));
        }
    return code;
}
void ArmyArtwork::validateRelocations(const Rom &rom) {
    if (rom.revision())
        return;
    if (rom.bytes().mid(descriptorOffset, 8) == "LC05ARMY") {
        load(rom);
        return;
    }
    for (const auto &spec : specs())
        for (const auto &[hi, lo] : spec.sites)
            if ((((u32(rom.bytes(), hi) & 65535) << 16) | (u32(rom.bytes(), lo) & 65535)) !=
                spec.retailKey) {
                load(rom);
                return;
            }
}
ArmyArtwork ArmyArtwork::load(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Army artwork currently requires US revision zero");
    ArmyArtwork result;
    for (int atlas = 0; atlas < 6; ++atlas) {
        const auto &spec = specs()[atlas];
        auto &pack = result.packs_[atlas];
        for (const auto &[hi, lo] : spec.sites) {
            const auto high = u32(rom.bytes(), hi), low = u32(rom.bytes(), lo),
                       key = resourceKey(high, low);
            if (pack.key && key != pack.key)
                throw Error(spec.label + " resource owners disagree");
            pack.key = key;
            result.ownerWords_[hi] = high;
            result.ownerWords_[lo] = low;
        }
        const auto at = pack.key + resourceBase, stored = u32(rom.bytes(), at),
                   decoded = u32(rom.bytes(), at + 4);
        const int plane = spec.width * spec.height;
        if (stored < 4 || stored > 65536 ||
            decoded < quint32(1024 + plane * spec.retailCount + spec.tail) ||
            decoded > quint32(1024 + plane * spec.capacity + spec.tail) ||
            (decoded - 1024 - spec.tail) % plane)
            throw Error(spec.label + " has unsupported decoded dimensions");
        pack.count = (int(decoded) - 1024 - spec.tail) / plane;
        pack.source = Resource::load(rom, at, stored + 4).decoded;
        for (int palette = 0; palette < 2; ++palette)
            for (int index = 0; index < 256; ++index)
                if (bool(u16(pack.source, palette * 512 + index * 2) & 1) ==
                    spec.transparent.contains(index))
                    throw Error(spec.label + " transparency indices changed");
    }
    result.descriptor_ = rom.bytes().mid(descriptorOffset, descriptorSize);
    if (result.descriptor_.startsWith("LC05ARMY")) {
        const auto &descriptor = result.descriptor_;
        if (descriptor.size() != descriptorSize || u32(descriptor, 8) != 1 ||
            descriptor.mid(20, 12) != QByteArray(12, 0) ||
            descriptor.mid(80, 48) != QByteArray(48, 0))
            throw Error("Unsupported native Army artwork descriptor");
        Placement p;
        p.offset = u32(descriptor, 12);
        p.allocation = u32(descriptor, 16);
        p.hash = descriptor.mid(128, 32);
        if (p.offset < 0x2800000 || (p.offset - resourceBase) % 65536 || !p.allocation ||
            p.allocation % 16 || p.allocation > 0x40000 ||
            quint64(p.offset) + p.allocation > quint64(rom.bytes().size()) ||
            quint64(p.offset) + p.allocation > 0x4000000 ||
            QByteArray::fromHex(sha256(rom.bytes().mid(p.offset, p.allocation))) != p.hash)
            throw Error("Native Army artwork allocation or hash is invalid");
        quint32 cursor = 0;
        for (int atlas = 0; atlas < 6; ++atlas) {
            p.relative[atlas] = u32(descriptor, 32 + atlas * 4);
            p.sizes[atlas] = u32(descriptor, 56 + atlas * 4);
            const quint32 alignment = atlas % 2 ? 16 : 65536,
                          next = (cursor + alignment - 1) & ~(alignment - 1);
            if (p.relative[atlas] != next || p.sizes[atlas] < 8 ||
                quint64(next) + p.sizes[atlas] > p.allocation ||
                result.packs_[atlas].key != p.offset + next - resourceBase ||
                u32(rom.bytes(), p.offset + next) + 4 != p.sizes[atlas] ||
                rom.bytes().mid(p.offset + cursor, next - cursor) !=
                    QByteArray(next - cursor, char(-1)))
                throw Error("Native Army artwork resources overlap or contain unclaimed data");
            if (atlas % 2 && result.packs_[atlas].key >> 16 != result.packs_[atlas - 1].key >> 16)
                throw Error("Army sprite sides no longer share their loader's upper operand");
            cursor = next + p.sizes[atlas];
        }
        if (p.allocation != ((cursor + 15U) & ~15U) ||
            rom.bytes().mid(p.offset + cursor, p.allocation - cursor) !=
                QByteArray(p.allocation - cursor, char(-1)))
            throw Error("Native Army artwork padding was modified");
        result.placement_ = p;
    }
    return result;
}
int ArmyArtwork::classLane(const ClassAsset &asset) {
    return u8(asset.record, ClassData::unitSizeOffset) == 2 ? 2 : (u8(asset.appearance, 4) & 128) ? 1 : 0;
}
int ArmyArtwork::classModel(const ClassAsset &asset) {
    return u8(asset.appearance, 4) & 63;
}
QVector<int> ArmyArtwork::consumers(const ClassData &data, int atlas, int model) const {
    validate(atlas, model);
    QVector<int> result;
    for (int id = 0; id < data.entries().size(); ++id)
        if (classLane(data.entries()[id]) == atlas / 2 && classModel(data.entries()[id]) == model)
            result.append(id);
    return result;
}
int ArmyArtwork::sourceCount(int atlas) const {
    validate(atlas);
    return packs_[atlas].count;
}
bool ArmyArtwork::present(int atlas, int model) const {
    validate(atlas, model);
    return model < packs_[atlas].count || modified(atlas, model);
}
QByteArray ArmyArtwork::indices(int atlas, int model) const {
    const auto &spec = validate(atlas, model);
    const auto found = edits_.constFind(atlas * 64 + model);
    if (found != edits_.cend())
        return found.value();
    if (model >= packs_[atlas].count)
        return {};
    return packs_[atlas].source.mid(1024 + model * spec.width * spec.height,
                                    spec.width * spec.height);
}
QVector<quint16> ArmyArtwork::palette(int atlas, int ordinal) const {
    validate(atlas);
    if (ordinal < 0 || ordinal > 1)
        throw Error("Army palette is outside its pair");
    QVector<quint16> result;
    for (int i = 0; i < 256; ++i)
        result.append(u16(packs_[atlas].source, ordinal * 512 + i * 2));
    return result;
}
QByteArray ArmyArtwork::words(int atlas, int model, int ordinal) const {
    const auto pixels = indices(atlas, model);
    const auto colors = palette(atlas, ordinal);
    QByteArray result(pixels.size() * 2, 0);
    for (int i = 0; i < pixels.size(); ++i)
        put16(result, i * 2, colors[quint8(pixels[i])]);
    return result;
}
QImage ArmyArtwork::image(int atlas, int model, int ordinal) const {
    const auto &spec = validate(atlas, model);
    const auto pixels = words(atlas, model, ordinal);
    QImage result(spec.width, spec.height, QImage::Format_RGBA8888);
    result.fill(Qt::transparent);
    if (!pixels.isEmpty())
        for (int y = 0; y < spec.height; ++y)
            for (int x = 0; x < spec.width; ++x)
                result.setPixelColor(x, y, color(u16(pixels, (y * spec.width + x) * 2)));
    return result;
}
QByteArray ArmyArtwork::mapImage(int atlas, int ordinal, const QImage &image, int threshold,
                                 const QByteArray &previous) const {
    const auto &spec = validate(atlas);
    const auto colors = palette(atlas, ordinal);
    if (image.size() != QSize(spec.width, spec.height) || threshold < 1 || threshold > 255 ||
        (!previous.isEmpty() && previous.size() != spec.width * spec.height))
        throw Error("Army image dimensions or transparency threshold are invalid");
    QByteArray result(spec.width * spec.height, 0);
    QMap<QRgb, int> cache;
    for (int y = 0; y < spec.height; ++y)
        for (int x = 0; x < spec.width; ++x) {
            const auto value = image.pixelColor(x, y);
            int chosen = spec.transparent.first();
            if (!previous.isEmpty()) {
                const auto old = quint8(previous[y * spec.width + x]);
                if ((value.alpha() < threshold && !(colors[old] & 1)) ||
                    (value.alpha() >= threshold && color(colors[old]) == value)) {
                    result[y * spec.width + x] = char(old);
                    continue;
                }
            }
            if (value.alpha() >= threshold) {
                const auto rgb = value.rgb();
                if (cache.contains(rgb))
                    chosen = cache[rgb];
                else {
                    int best = std::numeric_limits<int>::max();
                    for (int i = 0; i < 256; ++i)
                        if (colors[i] & 1) {
                            const auto c = color(colors[i]);
                            const int r = c.red() - value.red(), g = c.green() - value.green(),
                                      b = c.blue() - value.blue(),
                                      distance = 77 * r * r + 150 * g * g + 29 * b * b;
                            if (distance < best) {
                                best = distance;
                                chosen = i;
                            }
                        }
                    cache[rgb] = chosen;
                }
            }
            result[y * spec.width + x] = char(chosen);
        }
    return result;
}
void ArmyArtwork::setIndices(int atlas, int model, const QByteArray &pixels) {
    const auto &spec = validate(atlas, model);
    if (pixels.size() != spec.width * spec.height)
        throw Error("Army plane dimensions are invalid");
    const auto original =
        model < packs_[atlas].count
            ? packs_[atlas].source.mid(1024 + model * pixels.size(), pixels.size())
            : QByteArray{};
    if (pixels == original)
        edits_.remove(atlas * 64 + model);
    else
        edits_[atlas * 64 + model] = pixels;
}
void ArmyArtwork::restore(int atlas, int model) {
    validate(atlas, model);
    edits_.remove(atlas * 64 + model);
}
QJsonObject ArmyArtwork::json() const {
    QJsonObject planes;
    for (auto it = edits_.cbegin(); it != edits_.cend(); ++it)
        planes[specs()[it.key() / 64].slug + ":" + QString::number(it.key() % 64)] =
            QString::fromLatin1(it.value().toBase64());
    return {{"version", 1}, {"planes", planes}};
}
void ArmyArtwork::apply(const QJsonObject &value) {
    if (value["version"].toInt() != 1 || !value["planes"].isObject())
        throw Error("Unsupported Army artwork project");
    auto next = *this;
    next.edits_.clear();
    const auto planes = value["planes"].toObject();
    for (auto it = planes.begin(); it != planes.end(); ++it) {
        const auto split = it.key().lastIndexOf(':');
        bool ok = false;
        const auto model = it.key().mid(split + 1).toInt(&ok);
        int atlas = -1;
        for (int i = 0; i < 6; ++i)
            if (specs()[i].slug == it.key().left(split))
                atlas = i;
        if (!ok || atlas < 0 || it.key() != specs()[atlas].slug + ":" + QString::number(model))
            throw Error("Invalid Army plane identity");
        const auto &spec = validate(atlas, model);
        next.setIndices(atlas, model, decodeBase64(it.value(), spec.width * spec.height));
    }
    *this = std::move(next);
}
void ArmyArtwork::importLegacy(const QJsonObject &value) {
    const auto version = value["schemaVersion"].toInt();
    if (version < 1 || version > 5 || !value["armySprites"].isObject() ||
        (version < 4 && !value["armySprites"].toObject().isEmpty()))
        throw Error("Unsupported imported Army artwork");
    auto next = *this;
    const auto planes = value["armySprites"].toObject();
    for (auto it = planes.begin(); it != planes.end(); ++it) {
        const auto row = it.value().toObject();
        int atlas = -1;
        for (int i = 0; i < 6; ++i)
            if (row["atlas"] == specs()[i].slug)
                atlas = i;
        const auto model = row["modelId"].toInt(-1);
        const auto &spec = validate(atlas, model);
        if (model < 0 ||
            it.key() != spec.slug + ":" + QString("%1").arg(model, 2, 16, QChar('0')) ||
            row["width"].toInt() != spec.width || row["height"].toInt() != spec.height ||
            row["resourceKey"].toString() != QString("0x%1")
                                                 .arg(spec.retailKey, 8, 16, QChar('0'))
                                                 .toUpper()
                                                 .replace("0X", "0x") ||
            (row.contains("retailPlane") &&
             (!row["retailPlane"].isBool() ||
              row["retailPlane"].toBool() != (model < spec.retailCount))) ||
            (model >= spec.retailCount && version < 5) ||
            decodeBase64(row["paletteRgba5551BeBase64"], 1024) != packs_[atlas].source.first(1024))
            throw Error("Imported Army plane disagrees with its source atlas and palettes");
        next.setIndices(atlas, model,
                        decodeBase64(row["ci8IndicesBase64"], spec.width * spec.height));
    }
    *this = std::move(next);
}
std::array<QByteArray, 6> ArmyArtwork::decodedPacks() const {
    std::array<QByteArray, 6> result;
    for (int atlas = 0; atlas < 6; ++atlas) {
        const auto &spec = specs()[atlas];
        const auto &pack = packs_[atlas];
        const auto plane = spec.width * spec.height;
        bool expand = false;
        for (int model = pack.count; model < spec.capacity; ++model)
            expand |= modified(atlas, model);
        auto data = pack.source;
        if (expand) {
            data =
                pack.source.first(1024 + pack.count * plane) +
                QByteArray((spec.capacity - pack.count) * plane, char(spec.transparent.first())) +
                pack.source.last(spec.tail);
        }
        for (int model = 0; model < spec.capacity; ++model)
            if (modified(atlas, model))
                data.replace(1024 + model * plane, plane, edits_.value(atlas * 64 + model));
        result[atlas] = std::move(data);
    }
    return result;
}
} // namespace lordly
