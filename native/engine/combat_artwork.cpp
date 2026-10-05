#include "engine/combat_artwork.h"
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <limits>
namespace lordly {
namespace {
QByteArray resource(const QByteArray &rom, quint32 key, bool compressed) {
    if (!key || key % 2 || key >= 0x4000000 - CombatArtwork::resourceBase)
        throw Error("Invalid combat artwork resource key");
    const auto at = key + CombatArtwork::resourceBase, size = u32(rom, at);
    if (size > 16 * 1024 * 1024 || quint64(at) + 4 + size > quint64(rom.size()))
        throw Error("Combat artwork resource exceeds its ROM");
    if (!compressed)
        return rom.mid(at + 4, size);
    if (size < 4 || u32(rom, at + 4) > 16 * 1024 * 1024)
        throw Error("Combat artwork exceeds its decode budget");
    qsizetype used = 0;
    const auto data = decodeLzss(rom.mid(at + 8, size - 4), u32(rom, at + 4), &used);
    if (used != size - 4)
        throw Error("Combat artwork has unconsumed compressed bytes");
    return data;
}
QByteArray base64(const QJsonValue &value, qsizetype size) {
    if (!value.isString() || value.toString().size() > (size + 2) / 3 * 4)
        throw Error("Invalid combat sprite pixel encoding");
    const auto text = value.toString().toLatin1();
    const auto bytes = QByteArray::fromBase64(text, QByteArray::AbortOnBase64DecodingErrors);
    if (bytes.size() != size || bytes.toBase64() != text)
        throw Error("Combat sprite pixels have the wrong size or encoding");
    return bytes;
}
quint32 hexKey(const QJsonValue &value) {
    bool ok = false;
    const auto text = value.toString();
    auto key = text.toUInt(&ok, 16);
    if (!ok || !QRegularExpression("^(0x)?[0-9a-fA-F]{8}$").match(text).hasMatch())
        throw Error("Invalid combat artwork resource identity");
    return key;
}
int ordinal(const QString &text) {
    bool ok = false;
    int value = text.toInt(&ok);
    if (!ok || value < 0 || value > 65535 || QString::number(value) != text)
        throw Error("Invalid combat artwork child ordinal");
    return value;
}
QString hex(quint32 value) {
    return QString("0x%1").arg(value, 8, 16, QChar('0'));
}
} // namespace
QString CombatArtwork::identity(quint32 descriptor, int art) {
    return QString("%1:%2").arg(descriptor, 8, 16, QChar('0')).arg(art);
}
CombatArtwork CombatArtwork::load(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Combat artwork currently requires US revision 0");
    CombatArtwork result;
    result.source_ = rom.bytes();
    result.descriptor_ = rom.bytes().mid(descriptorOffset, descriptorSize);
    const auto directory = resource(rom.bytes(), 0x3b6cd0, false);
    if (directory.isEmpty() || directory.size() % 4 || directory.size() > 4095 * 4)
        throw Error("Invalid combat artwork directory");
    QMap<quint32, QVector<int>> handles;
    for (int at = 0; at < directory.size(); at += 4)
        if (u32(directory, at))
            handles[u32(directory, at)] << at / 4 + 1;
    for (auto it = handles.cbegin(); it != handles.cend(); ++it) {
        const auto key = it.key();
        const auto entries = resource(rom.bytes(), key, false);
        if (entries.size() < 16 || entries.size() % 4 || entries.size() > 65540 * 4)
            throw Error("Invalid combat artwork descriptor");
        const int count = int(entries.size() / 4 - 4);
        const auto config = resource(rom.bytes(), u32(entries, 8), true);
        const auto palettes = resource(rom.bytes(), u32(entries, 12), true);
        const auto map = u32(config, 0x1c);
        if (u32(config, 0) != quint32(count) || quint64(map) + count > quint64(config.size()) ||
            config.size() < 0x20 + count || palettes.size() % 512)
            throw Error("Combat artwork configuration does not match its descriptor");
        for (int art = 0; art < count; ++art) {
            const auto resourceKey = u32(entries, (art + 4) * 4);
            if (!resourceKey)
                continue;
            const int bank = u8(config, map + art), policy = u8(config, 0x20 + art);
            if (policy > 2)
                throw Error("Combat artwork has an invalid selection policy");
            if (result.bindings_.size() >= 65536)
                throw Error("Combat artwork exceeds its binding budget");
            const auto id = identity(key, art);
            result.bindings_[id] = {id,
                                    key,
                                    resourceKey,
                                    key + resourceBase + 4U + quint32((art + 4) * 4),
                                    art,
                                    policy,
                                    bank,
                                    it.value(),
                                    palettes.mid(bank * 512, 512)};
        }
    }
    if (result.descriptor_.startsWith("LC05CMBA")) {
        const auto &d = result.descriptor_;
        if (d.size() != descriptorSize || u32(d, 8) != 1 || d.mid(24, 40) != QByteArray(40, 0))
            throw Error("Invalid native combat artwork allocation header");
        result.allocationOffset_ = u32(d, 12);
        result.allocationSize_ = u32(d, 16);
        const auto count = u32(d, 20);
        if (!count || count > quint32(result.bindings_.size()) ||
            result.allocationOffset_ < 0x2800000 || result.allocationOffset_ % 16 ||
            result.allocationSize_ % 16 || quint64(count) * 16 > result.allocationSize_ ||
            quint64(result.allocationOffset_) + result.allocationSize_ >
                quint64(rom.bytes().size()))
            throw Error("Native combat artwork allocation exceeds its ROM");
        const auto data = rom.bytes().mid(result.allocationOffset_, result.allocationSize_);
        if (QByteArray::fromHex(sha256(data)) != d.mid(64, 32))
            throw Error("Native combat artwork allocation hash differs");
        quint32 next = count * 16;
        quint64 retained = 0;
        for (quint32 i = 0; i < count; ++i) {
            const auto id = identity(u32(data, i * 16), int(u32(data, i * 16 + 4)));
            const auto relative = u32(data, i * 16 + 8), length = u32(data, i * 16 + 12);
            const auto &binding = result.binding(id);
            if (result.owned_.contains(id) || relative != next || length < 8 ||
                quint64(relative) + length > quint64(data.size()) ||
                binding.resourceKey != result.allocationOffset_ + relative - resourceBase ||
                u32(data, relative) != length - 4)
                throw Error("Native combat artwork allocation has invalid members or references");
            result.owned_[id] = resource(rom.bytes(), binding.resourceKey, true);
            retained += result.owned_[id].size();
            if (retained > 512ULL * 1024 * 1024)
                throw Error("Combat artwork exceeds its retained data budget");
            const auto end = relative + length;
            next = (end + 15) & ~15U;
            if (data.mid(end, next - end) != QByteArray(next - end, char(-1)))
                throw Error("Native combat artwork allocation padding differs");
        }
        if (next != quint32(data.size()))
            throw Error("Native combat artwork has an unclaimed tail");
    }
    return result;
}
const CombatArtBinding &CombatArtwork::binding(const QString &id) const {
    const auto it = bindings_.constFind(id);
    if (it == bindings_.cend())
        throw Error("Unknown combat artwork descriptor member");
    return it.value();
}
SpriteObject CombatArtwork::original(const QString &id) const {
    const auto key = binding(id).resourceKey;
    if (!cache_.contains(key)) {
        if (cache_.size() >= 32)
            cache_.clear();
        cache_[key] = SpriteObject::decode(resource(source_, key, true));
    }
    return cache_.value(key);
}
SpriteObject CombatArtwork::current(const QString &id) const {
    auto sprite = original(id);
    return edits_.contains(id) ? SpriteObject::decode(sprite.rebuild(edits_.value(id))) : sprite;
}
SpritePixels CombatArtwork::pixels(const QString &id, int child) const {
    return edits_.value(id).contains(child) ? edits_.value(id).value(child)
                                            : original(id).pixels(child);
}
QByteArray CombatArtwork::palette(const QString &id, int child) const {
    const auto embedded = original(id).materialize(child).palette;
    return embedded.isEmpty() ? binding(id).palette : embedded;
}
QImage CombatArtwork::image(const QString &id, int child) const {
    const auto sprite = current(id);
    if (child < 0 || child >= sprite.children().size())
        throw Error("Invalid combat sprite child");
    const auto frame = sprite.frame(child, binding(id).palette).image;
    return QImage(reinterpret_cast<const uchar *>(frame.rgba.constData()), frame.width,
                  frame.height, frame.width * 4, QImage::Format_RGBA8888)
        .copy();
}
void CombatArtwork::setPixels(const QString &id, int child, const SpritePixels &pixels) {
    const auto sprite = original(id);
    const auto before = sprite.pixels(child);
    sprite.rebuild({{child, pixels}}); // Validate the complete dependent-child reconstruction.
    if (before == pixels)
        restore(id, child);
    else
        edits_[id][child] = pixels;
}
void CombatArtwork::restore(const QString &id, int child) {
    original(id).pixels(child);
    if (edits_.contains(id)) {
        edits_[id].remove(child);
        if (edits_[id].isEmpty())
            edits_.remove(id);
    }
}
QJsonObject CombatArtwork::json() const {
    QJsonObject groups;
    for (auto it = edits_.cbegin(); it != edits_.cend(); ++it) {
        QJsonObject children;
        for (auto child = it->cbegin(); child != it->cend(); ++child)
            children[QString::number(child.key())] =
                QJsonObject{{"color", QString::fromLatin1(child->color.toBase64())},
                            {"alpha", QString::fromLatin1(child->alpha.toBase64())}};
        const auto sprite = original(it.key());
        groups[it.key()] =
            QJsonObject{{"resourceKey", hex(binding(it.key()).resourceKey)},
                        {"sourceSha256", QString::fromLatin1(sha256(sprite.source()))},
                        {"children", children}};
    }
    return {{"version", 1}, {"bindings", groups}};
}
void CombatArtwork::apply(const QJsonObject &object) {
    if (object["version"].toInt() != 1 || !object["bindings"].isObject())
        throw Error("Invalid native combat artwork document");
    auto next = *this;
    next.edits_.clear();
    const auto groups = object["bindings"].toObject();
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        const auto group = it.value().toObject();
        const auto sprite = original(it.key());
        if (!it.value().isObject() ||
            hexKey(group["resourceKey"]) != binding(it.key()).resourceKey ||
            group["sourceSha256"].toString().toLatin1() != sha256(sprite.source()) ||
            !group["children"].isObject() || group["children"].toObject().isEmpty())
            throw Error("Combat artwork document does not match its current ROM source");
        const auto children = group["children"].toObject();
        for (auto child = children.begin(); child != children.end(); ++child) {
            const int index = ordinal(child.key());
            const auto original = sprite.pixels(index);
            const auto entry = child.value().toObject();
            next.setPixels(it.key(), index,
                           {base64(entry["color"], original.color.size()),
                            base64(entry["alpha"], original.alpha.size())});
        }
    }
    *this = std::move(next);
}
QMap<QString, QByteArray> CombatArtwork::decodedResources() const {
    QMap<QString, QByteArray> result;
    for (auto it = edits_.cbegin(); it != edits_.cend(); ++it)
        result[it.key()] = original(it.key()).rebuild(it.value());
    return result;
}
SpritePixels CombatArtwork::mapImage(const QString &id, int child, const QImage &image) const {
    const auto sprite = original(id);
    if (image.size() != QSize(sprite.width(), sprite.height()))
        throw Error("Imported combat artwork must match its stored dimensions");
    auto result = pixels(id, child);
    const auto colors = palette(id, child);
    const int bytes = sprite.colorFormat() == 1 ? 1 : sprite.colorFormat() == 2 ? 2 : 4;
    auto expand = [](int value) { return (value * 255 + 15) / 31; };
    const auto old = this->image(id, child);
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            const int pixel = y * image.width() + x;
            const auto color = image.pixelColor(x, y);
            if (!result.alpha.isEmpty())
                put8(result.alpha, pixel,
                     quint8(sprite.alphaFormat() ? color.alpha() : (color.alpha() + 8) / 17));
            // Keep duplicate palette indices when the visible color is unchanged.
            if (color.rgb() == old.pixelColor(x, y).rgb() &&
                (!result.alpha.isEmpty() || color.alpha() == old.pixelColor(x, y).alpha()))
                continue;
            if (sprite.colorFormat() == 1) {
                int best = 0, distance = std::numeric_limits<int>::max();
                for (int i = 0; i < 256; ++i) {
                    const auto word = u16(colors, i * 2);
                    const int r = color.red() - expand(word >> 11),
                              g = color.green() - expand((word >> 6) & 31),
                              b = color.blue() - expand((word >> 1) & 31),
                              a = color.alpha() - ((word & 1) ? 255 : 0);
                    const int score = 77 * r * r + 150 * g * g + 29 * b * b +
                                      (result.alpha.isEmpty() ? 256 * a * a : 0);
                    if (score < distance) {
                        best = i;
                        distance = score;
                    }
                }
                put8(result.color, pixel, quint8(best));
            } else if (sprite.colorFormat() == 2) {
                const auto word =
                    quint16((((color.red() * 31 + 127) / 255) << 11) |
                            (((color.green() * 31 + 127) / 255) << 6) |
                            (((color.blue() * 31 + 127) / 255) << 1) |
                            (result.alpha.isEmpty() ? (color.alpha() >= 128 ? 1 : 0)
                                                    : u16(result.color, pixel * 2) & 1));
                put16(result.color, pixel * bytes, word);
            } else {
                put8(result.color, pixel * 4, quint8(color.red()));
                put8(result.color, pixel * 4 + 1, quint8(color.green()));
                put8(result.color, pixel * 4 + 2, quint8(color.blue()));
                if (result.alpha.isEmpty())
                    put8(result.color, pixel * 4 + 3, quint8(color.alpha()));
            }
        }
    return result;
}
void CombatArtwork::importLegacy(const QJsonObject &art) {
    if (art["schemaVersion"].toInt() < 1 || art["schemaVersion"].toInt() > 5 ||
        !art["animations"].isObject())
        throw Error("Invalid browser combat artwork document");
    QFile aliases(":/game/art/combat-import.json");
    if (!aliases.open(QIODevice::ReadOnly))
        throw Error("Missing combat project import vocabulary");
    const auto animations =
        QJsonDocument::fromJson(aliases.readAll()).object()["animations"].toObject();
    auto next = *this;
    next.edits_.clear();
    QSet<QString> seen;
    const auto groups = art["animations"].toObject();
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        const auto group = it.value().toObject();
        const auto animation = group["animation"].toString();
        const auto knownDescriptor = animations.value(animation);
        const auto descriptor = group.contains("descriptorKey") ? hexKey(group["descriptorKey"])
                                                                : hexKey(knownDescriptor);
        if (!knownDescriptor.isUndefined() && hexKey(knownDescriptor) != descriptor)
            throw Error("Imported animation does not match its descriptor");
        const int artId = group["artId"].toInt(-1);
        const auto id = identity(descriptor, artId);
        const auto &row = binding(id);
        const auto sprite = original(id);
        if (hexKey(group["resourceKey"]) != row.resourceKey ||
            group["width"].toInt() != sprite.width() ||
            group["height"].toInt() != sprite.height() || sprite.colorFormat() != 1 ||
            sprite.alphaFormat() != 0 ||
            (group.contains("descriptorMemberOrdinal") &&
             group["descriptorMemberOrdinal"].toInt() != artId + 4) ||
            (group.contains("sourceRole") &&
             group["sourceRole"].toString() !=
                 QStringList{"body", "equipment", "element-effect"}[row.policy]))
            throw Error("Imported combat pixels do not match their current ROM binding");
        auto children = group["children"].toObject();
        if (!group.contains("children") && group["childOrdinal"].isDouble())
            children[QString::number(group["childOrdinal"].toInt(-1))] = group;
        if (children.isEmpty())
            throw Error("Imported combat artwork contains no edited children");
        for (auto child = children.begin(); child != children.end(); ++child) {
            const auto index = ordinal(child.key());
            const auto unique = id + ":" + child.key();
            if (seen.contains(unique))
                throw Error("Imported combat artwork repeats a child binding");
            seen.insert(unique);
            const auto entry = child.value().toObject();
            const auto original = sprite.pixels(index);
            next.setPixels(id, index,
                           {base64(entry["ci8IndicesBase64"], original.color.size()),
                            base64(entry["i4IntensityBase64"], original.alpha.size())});
        }
    }
    *this = std::move(next);
}
} // namespace lordly
