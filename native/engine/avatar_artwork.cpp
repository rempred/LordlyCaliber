#include "engine/avatar_artwork.h"
#include "engine/resource.h"
#include <QJsonArray>
#include <QSet>
namespace lordly {
namespace {
quint16 route(const ClassData &data, int id, int selector) {
    if (id < 0 || id >= data.entries().size() || selector < 0 || selector > 1)
        throw Error("Portrait selection is outside the current class table");
    return u16(data.entries()[id].appearance, selector * 2);
}
QByteArray pixels(const QJsonValue &value) {
    const auto text = value.toString().toLatin1();
    const auto bytes = QByteArray::fromBase64(text, QByteArray::AbortOnBase64DecodingErrors);
    if (!value.isString() || bytes.size() != 3840 || bytes.toBase64() != text)
        throw Error("A portrait needs exactly 1920 RGBA5551 pixel words");
    return bytes;
}
QByteArray container(const QByteArray &data) {
    const auto encoded = encodeLzss(data);
    if (decodeLzss(encoded, data.size()) != data)
        throw Error("Portrait compression failed readback");
    QByteArray result(8, 0);
    put32(result, 0, quint32(encoded.size() + 4));
    put32(result, 4, quint32(data.size()));
    return result + encoded;
}
void align(QByteArray &data) {
    data += QByteArray((16 - data.size() % 16) % 16, char(-1));
}
} // namespace
AvatarArtwork AvatarArtwork::load(const Rom &rom) {
    AvatarArtwork result;
    result.library_ = AvatarLibrary::load(rom);
    for (const auto &[hi, lo] : AvatarLibrary::sites()) {
        result.ownerWords_[hi] = u32(rom.bytes(), hi);
        result.ownerWords_[lo] = u32(rom.bytes(), lo);
    }
    result.descriptor_ =
        rom.bytes().mid(AvatarLibrary::descriptorOffset, AvatarLibrary::descriptorSize);
    return result;
}
QByteArray AvatarArtwork::words(const ClassData &data, int id, int selector) const {
    const auto token = route(data, id, selector);
    const auto key = id * 2 + selector;
    return edits_.contains(key) ? edits_.value(key) : library_.words(token);
}
QImage AvatarArtwork::render(const QByteArray &words) {
    QImage image(40, 48, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    if (words.isEmpty())
        return image;
    if (words.size() != 3840)
        throw Error("Invalid portrait dimensions");
    auto expand = [](int n) { return (255 * n + 15) / 31; };
    for (int y = 0; y < 48; ++y)
        for (int x = 0; x < 40; ++x) {
            const auto word = u16(words, (y * 40 + x) * 2);
            image.setPixelColor(x, y,
                                {expand(word >> 11), expand((word >> 6) & 31),
                                 expand((word >> 1) & 31), (word & 1) ? 255 : 0});
        }
    return image;
}
QImage AvatarArtwork::image(const ClassData &data, int id, int selector) const {
    return render(words(data, id, selector));
}
QByteArray AvatarArtwork::pack(const QByteArray &words) {
    if (words.size() != 3840)
        throw Error("Portraits require 40 by 48 pixels");
    QByteArray result(2080, 0);
    QMap<quint16, int> colors;
    for (int i = 0; i < 1920; ++i) {
        const auto word = u16(words, i * 2);
        if (!(word & 1))
            throw Error("Portraits require opaque colors");
        if (!colors.contains(word)) {
            if (colors.size() == 80)
                throw Error("A portrait can contain at most 80 colors");
            const auto index = int(colors.size());
            colors[word] = index;
            put16(result, 1920 + index * 2, word);
        }
        result[i] = char(colors[word]);
    }
    for (int i = int(colors.size()); i < 80; ++i)
        put16(result, 1920 + i * 2, 1);
    return result;
}
void AvatarArtwork::setWords(const ClassData &data, int id, int selector, const QByteArray &words) {
    const auto token = route(data, id, selector);
    pack(words);
    if (words == library_.words(token))
        edits_.remove(id * 2 + selector);
    else
        edits_[id * 2 + selector] = words;
}
void AvatarArtwork::restore(int id, int selector) {
    edits_.remove(id * 2 + selector);
}
QJsonObject AvatarArtwork::json() const {
    QJsonArray rows;
    for (auto it = edits_.cbegin(); it != edits_.cend(); ++it)
        rows.append(QJsonObject{{"classId", it.key() / 2},
                                {"selector", it.key() % 2},
                                {"pixels", QString::fromLatin1(it.value().toBase64())}});
    return {{"version", 1}, {"portraits", rows}};
}
void AvatarArtwork::apply(const ClassData &data, const QJsonObject &object) {
    if (object["version"].toInt() != 1 || !object["portraits"].isArray())
        throw Error("Unsupported portrait project schema");
    auto next = *this;
    next.edits_.clear();
    QSet<int> seen;
    for (const auto &value : object["portraits"].toArray()) {
        const auto row = value.toObject();
        const auto id = row["classId"].toInt(-1), selector = row["selector"].toInt(-1);
        route(data, id, selector);
        if (seen.contains(id * 2 + selector))
            throw Error("Duplicate portrait project selector");
        seen.insert(id * 2 + selector);
        next.setWords(data, id, selector, pixels(row["pixels"]));
    }
    *this = std::move(next);
}
void AvatarArtwork::importLegacy(const ClassData &data, const QJsonObject &object) {
    const auto version = object["schemaVersion"].toInt();
    if (version < 1 || version > 5 || !object["avatars"].isObject())
        throw Error("Unsupported imported portrait schema");
    auto next = *this;
    const auto rows = object["avatars"].toObject();
    for (auto it = rows.begin(); it != rows.end(); ++it) {
        if (!it.value().isObject())
            throw Error("Invalid imported portrait");
        const auto row = it.value().toObject();
        const auto id = row["classId"].toInt(-1);
        const auto selectors = row["selectorIndices"].toArray();
        if (selectors.isEmpty() || selectors.size() > 2)
            throw Error("Invalid imported portrait selectors");
        const auto words = pixels(row["pixelsRgba5551BeBase64"]);
        QStringList suffix;
        QSet<int> seen;
        for (const auto &value : selectors) {
            const auto selector = value.toInt(-1);
            route(data, id, selector);
            if (seen.contains(selector))
                throw Error("Duplicate imported portrait selector");
            seen.insert(selector);
            suffix.append(QString::number(selector));
            next.setWords(data, id, selector, words);
        }
        if (it.key() != QString::number(id) + ":" + suffix.join("-"))
            throw Error("Imported portrait identity disagrees with its selectors");
    }
    *this = std::move(next);
}
QMap<int, QByteArray> AvatarArtwork::decodedBundles() const {
    QMap<int, QByteArray> result;
    for (auto it = edits_.cbegin(); it != edits_.cend(); ++it)
        result[it.key()] = pack(it.value());
    return result;
}
QByteArray AvatarArtwork::rebuild(const Rom &rom, ClassData &classes,
                                  const QMap<int, QByteArray> &compiled) const {
    if (compiled != decodedBundles())
        throw Error("Compiled portraits differ from the project");
    if (!modified())
        return rom.bytes();
    const auto &prior = library_.placement();
    if (rom.bytes().mid(AvatarLibrary::descriptorOffset, AvatarLibrary::descriptorSize) !=
        descriptor_)
        throw Error("Another export owner changed the portrait descriptor");
    if (!prior && descriptor_ != QByteArray(AvatarLibrary::descriptorSize, char(-1)))
        throw Error("Portrait descriptor allocation is occupied");
    for (auto it = ownerWords_.cbegin(); it != ownerWords_.cend(); ++it)
        if (u32(rom.bytes(), it.key()) != it.value())
            throw Error("Another export owner changed a portrait resource operand");
    if (prior && QByteArray::fromHex(sha256(rom.bytes().mid(prior->offset, prior->allocation))) !=
                     prior->hash)
        throw Error("Another export owner changed the portrait allocation");
    auto members = library_.members();
    if (members.size() + compiled.size() > 8192)
        throw Error("Portrait directory exceeds its 13-bit member domain");
    QByteArray payload(4 + (members.size() + compiled.size()) * 4, 0);
    put32(payload, 0, quint32(payload.size() - 4));
    align(payload);
    QMap<quint32, quint32> moved;
    QMap<int, quint32> relative;
    // Carry every referenced owned resource forward before clearing its old allocation.
    for (int member = 1; member < members.size(); ++member) {
        const auto key = members[member], address = AvatarLibrary::resourceBase + key;
        if (prior && address >= prior->offset && address < prior->offset + prior->allocation) {
            if (!moved.contains(key)) {
                moved[key] = quint32(payload.size());
                payload += library_.storedBundle(member);
                align(payload);
            }
            relative[member] = moved[key];
        }
    }
    auto nextClasses = classes;
    for (auto it = compiled.cbegin(); it != compiled.cend(); ++it) {
        const auto id = it.key() / 2, selector = it.key() % 2;
        route(nextClasses, id, selector);
        const auto member = int(members.size());
        members.append(0);
        relative[member] = quint32(payload.size());
        payload += container(it.value());
        align(payload);
        auto asset = nextClasses.entries()[id];
        put16(asset.appearance, selector * 2, quint16(member << 3));
        nextClasses.set(id, asset);
    }
    const auto allocation = quint32(payload.size());
    const bool tail =
        prior && quint64(prior->offset) + prior->allocation == quint64(rom.bytes().size());
    const quint32 offset = prior && (allocation <= prior->allocation || tail)
                               ? prior->offset
                               : quint32((rom.bytes().size() + 15) & ~qsizetype(15));
    if (offset < 0x2800000 || quint64(offset) + allocation > 0x4000000)
        throw Error("Portrait resources exceed cartridge capacity");
    for (auto it = relative.cbegin(); it != relative.cend(); ++it)
        members[it.key()] = offset + it.value() - AvatarLibrary::resourceBase;
    for (int i = 0; i < members.size(); ++i)
        put32(payload, 4 + i * 4, members[i]);
    auto image = rom.bytes();
    if (prior)
        image.replace(prior->offset, prior->allocation, QByteArray(prior->allocation, char(-1)));
    if (tail && offset == prior->offset)
        image.resize(offset);
    if (quint64(image.size()) < quint64(offset) + allocation)
        image += QByteArray(offset + allocation - image.size(), char(-1));
    image.replace(offset, allocation, payload);
    const auto key = offset - AvatarLibrary::resourceBase;
    for (const auto &[hi, lo] : AvatarLibrary::sites()) {
        put32(image, hi, (u32(image, hi) & 0xffff0000) | (key >> 16));
        put32(image, lo, (u32(image, lo) & 0xffff0000) | (key & 65535));
    }
    QByteArray descriptor(AvatarLibrary::descriptorSize, 0);
    descriptor.replace(0, 8, "LC05AVTR");
    put32(descriptor, 8, 1);
    put32(descriptor, 12, offset);
    put32(descriptor, 16, allocation);
    put32(descriptor, 20, quint32(members.size()));
    descriptor.replace(64, 32, QByteArray::fromHex(sha256(payload)));
    image.replace(AvatarLibrary::descriptorOffset, AvatarLibrary::descriptorSize, descriptor);
    const auto reopened = AvatarLibrary::load(Rom::fromBytes(image));
    for (int id = 0; id < classes.entries().size(); ++id)
        for (int selector = 0; selector < 2; ++selector)
            if (reopened.words(route(nextClasses, id, selector)) != words(classes, id, selector))
                throw Error("Rebuilt portrait readback differs from the project");
    classes = std::move(nextClasses);
    return image;
}
} // namespace lordly
