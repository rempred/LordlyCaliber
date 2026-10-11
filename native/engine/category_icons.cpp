#include "engine/category_icons.h"
#include "engine/promotion.h"
#include "engine/resource.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <limits>
namespace lordly {
namespace {
constexpr quint32 resourceBase = 0x594280, retailKey = 0x1dc873e;
quint32 key(quint32 high, quint32 low) {
    const auto result = ((high & 65535) << 16) | (low & 65535);
    if ((high >> 16) != 0x3c04 || (low >> 16) != 0x3484 || !result || result >= 0x10000000 ||
        (result & 1))
        throw Error("Unsupported Army menu artwork resource link");
    return result;
}
QByteArray loadPack(const Rom &rom, quint32 resourceKey, int expected) {
    const qsizetype at = resourceBase + resourceKey;
    const auto length = u32(rom.bytes(), at);
    if (length < 4 || length > 1024 * 1024 ||
        (expected ? u32(rom.bytes(), at + 4) != quint32(expected)
                  : (u32(rom.bytes(), at + 4) < 23496 || u32(rom.bytes(), at + 4) > 262144)))
        throw Error("Unsupported Army menu artwork dimensions");
    return Resource::load(rom, at, length + 4).decoded;
}
void checkId(int id) {
    if (id < 1 || id > 8)
        throw Error("Choose one of the eight class category icons");
}
} // namespace
QString CategoryIcons::name(int id) {
    checkId(id);
    static const QStringList names{"Male",   "Female", "Skull",  "Feather",
                                   "Dragon", "Paw",    "Helmet", "Horned face"};
    return names[id - 1];
}
QByteArray CategoryIcons::canonicalOwner(QByteArray code, qsizetype start) {
    if (ownerHigh >= start && ownerLow + 4 <= start + code.size()) {
        const auto hi = u32(code, ownerHigh - start), lo = u32(code, ownerLow - start);
        key(hi, lo);
        put32(code, ownerHigh - start, (hi & 0xffff0000) | (retailKey >> 16));
        put32(code, ownerLow - start, (lo & 0xffff0000) | (retailKey & 65535));
    }
    return code;
}
void CategoryIcons::validateRelocations(const Rom &rom) {
    if (!rom.revision() &&
        (key(u32(rom.bytes(), ownerHigh), u32(rom.bytes(), ownerLow)) != retailKey ||
         rom.bytes().mid(descriptorOffset, 8) == "LC05CATS"))
        load(rom);
}
CategoryIcons CategoryIcons::load(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Class category icons require US revision zero");
    CategoryIcons result;
    result.high_ = u32(rom.bytes(), ownerHigh);
    result.low_ = u32(rom.bytes(), ownerLow);
    result.key_ = key(result.high_, result.low_);
    result.source_ = loadPack(rom, result.key_, 0);
    result.loadedPack_ = result.source_;
    const auto at = resourceBase + result.key_;
    result.sourceContainer_ = rom.bytes().mid(at, u32(rom.bytes(), at) + 4);
    const auto palette =
        loadPack(rom, key(u32(rom.bytes(), 0xaed60), u32(rom.bytes(), 0xaed64)), 2336);
    for (int i = 0; i < 256; ++i) {
        const auto word = u16(palette, 0x5e0 + i * 2);
        if (!(word & 1))
            throw Error("Class category icons require the opaque menu palette");
        result.palette_ << QColor(((word >> 11) * 255 + 15) / 31,
                                  (((word >> 6) & 31) * 255 + 15) / 31,
                                  (((word >> 1) & 31) * 255 + 15) / 31);
    }
    result.descriptor_ = rom.bytes().mid(descriptorOffset, descriptorSize);
    if (result.descriptor_.startsWith("LC05CATS")) {
        const auto &d = result.descriptor_;
        result.placement_ = u32(d, 12);
        result.allocation_ = u32(d, 16);
        const quint64 end = quint64(result.placement_) + result.allocation_;
        if (u32(d, 8) != 1 || result.placement_ < 0x2800000 || result.placement_ % 16 ||
            result.allocation_ != ((result.sourceContainer_.size() + 15) & ~15) ||
            end > quint64(rom.bytes().size()) || result.key_ != result.placement_ - resourceBase ||
            u32(d, 20) > 1 || d.mid(24, 8) != QByteArray(8, 0) ||
            d.mid(32, 32) != QByteArray::fromHex(
                                 sha256(rom.bytes().mid(result.placement_, result.allocation_))) ||
            rom.bytes().mid(result.placement_ + result.sourceContainer_.size(),
                            result.allocation_ - result.sourceContainer_.size()) !=
                QByteArray(result.allocation_ - result.sourceContainer_.size(), char(-1)))
            throw Error("Class category artwork allocation is invalid or has been modified");
    }
    if (result.source_.size() > 23496) {
        const auto extra = result.source_.sliced(23496);
        if (extra.size() < 528 || extra.first(8) != "LCCATTAB")
            throw Error("Invalid family icon data");
        const auto metadata = u32(extra, 520), length = u32(extra, 524);
        if (metadata < 528 || quint64(metadata) + length != quint64(extra.size()))
            throw Error("Invalid family icon catalog");
        result.savedClassTabs_ = extra.mid(8, 256);
        result.apply(QJsonDocument::fromJson(extra.mid(metadata, length)).object());
        result.source_.truncate(23496);
        // Native pixels are already present in the loaded resource.
        result.edits_.clear();
        if (result.decodedPack().sliced(23496) != extra)
            throw Error("Family icon tables disagree with their catalog");
    }
    if ((result.hasTabRuntime() && result.loadedPack_.size() <= 23496) ||
        (result.hasFamilyTabs() && !result.hasTabRuntime()))
        throw Error("Family tab runtime and artwork data disagree");
    result.baseline_ = result.json();
    return result;
}
QString CategoryIcons::label(int id) const {
    if (id <= 8)
        return name(id);
    if (!names_.contains(id))
        throw Error("Unknown family icon");
    return names_[id];
}
QList<int> CategoryIcons::iconIds() const {
    QList<int> ids{1, 2, 3, 4, 5, 6, 7, 8};
    ids.append(custom_.keys());
    return ids;
}
int CategoryIcons::create(const QString &name, int copyFrom) {
    const auto trimmed = name.trimmed();
    if (trimmed.isEmpty() || trimmed.size() > 80)
        throw Error("Name the icon using 1–80 characters");
    const int id = custom_.isEmpty() ? 9 : custom_.lastKey() + 1;
    if (id > 255)
        throw Error("The icon library supports 255 icons");
    QImage blank(14, 14, QImage::Format_RGBA8888);
    blank.fill(Qt::white);
    const auto pixels = copyFrom ? indices(copyFrom) : mapImage(blank);
    names_[id] = trimmed;
    custom_[id] = pixels;
    return id;
}
void CategoryIcons::assignFamily(int family, int icon) {
    if (family < 1 || family > 65534)
        throw Error("Choose a class family first");
    if (icon) {
        label(icon);
        familyIcons_[family] = icon;
    } else
        familyIcons_.remove(family);
    if (familyIcons_.size() > 247) {
        familyIcons_.remove(family);
        throw Error("List Classes supports 247 family tabs plus the eight original categories");
    }
}
void CategoryIcons::validateFamilies(const Promotion &promotion) const {
    QSet<int> known;
    for (const auto &v : promotion.families["families"].toArray())
        known.insert(v.toObject()["id"].toInt());
    for (auto i = familyIcons_.cbegin(); i != familyIcons_.cend(); ++i)
        if (!known.contains(i.key()))
            throw Error("An icon is assigned to a missing class family");
}
QByteArray CategoryIcons::indices(int id) const {
    label(id);
    if (custom_.contains(id))
        return custom_[id];
    if (edits_.contains(id))
        return edits_[id];
    QByteArray result;
    for (int y = 0; y < 14; ++y)
        result += source_.mid(pixelOffset + y * 112 + (id - 1) * 14, 14);
    return result;
}
QImage CategoryIcons::image(int id) const {
    return image(indices(id));
}
QImage CategoryIcons::image(const QByteArray &pixels) const {
    if (pixels.size() != 196)
        throw Error("Category icons must be 14 × 14 pixels");
    QImage result(14, 14, QImage::Format_RGBA8888);
    for (int y = 0; y < 14; ++y)
        for (int x = 0; x < 14; ++x)
            result.setPixelColor(x, y, palette_[quint8(pixels[y * 14 + x])]);
    return result;
}
QByteArray CategoryIcons::mapImage(const QImage &source) const {
    if (source.size() != QSize(14, 14))
        throw Error("Category icons must be 14 × 14 pixels");
    QByteArray result(196, 0);
    for (int y = 0; y < 14; ++y)
        for (int x = 0; x < 14; ++x) {
            const auto c = source.pixelColor(x, y);
            // The native tab strip is opaque. Transparent pixels use its cream background.
            const QColor background(Qt::white);
            const int alpha = c.alpha();
            const QColor target((c.red() * alpha + background.red() * (255 - alpha)) / 255,
                                (c.green() * alpha + background.green() * (255 - alpha)) / 255,
                                (c.blue() * alpha + background.blue() * (255 - alpha)) / 255);
            int best = 0, distance = std::numeric_limits<int>::max();
            for (int i = 0; i < 256; ++i) {
                const auto p = palette_[i];
                const int r = target.red() - p.red(), g = target.green() - p.green(),
                          b = target.blue() - p.blue();
                const int d = r * r + g * g + b * b;
                if (d < distance) {
                    best = i;
                    distance = d;
                }
            }
            result[y * 14 + x] = char(best);
        }
    return result;
}
void CategoryIcons::setIndices(int id, const QByteArray &pixels) {
    label(id);
    if (pixels.size() != 196)
        throw Error("Category icons must be 14 × 14 pixels");
    if (id > 8) {
        custom_[id] = pixels;
        return;
    }
    auto candidate = *this;
    candidate.edits_.remove(id);
    if (candidate.indices(id) != pixels)
        candidate.edits_[id] = pixels;
    *this = std::move(candidate);
}
void CategoryIcons::restore(int id) {
    label(id);
    if (id <= 8)
        edits_.remove(id);
    else {
        const auto original = baseline_["custom"].toObject()[QString::number(id)].toObject();
        QImage blank(14, 14, QImage::Format_RGBA8888);
        blank.fill(Qt::white);
        custom_[id] = original.isEmpty()
                          ? mapImage(blank)
                          : QByteArray::fromBase64(original["pixels"].toString().toLatin1());
    }
}
QJsonObject CategoryIcons::json() const {
    QJsonObject edits;
    for (auto i = edits_.cbegin(); i != edits_.cend(); ++i)
        edits[QString::number(i.key())] = QString::fromLatin1(i.value().toBase64());
    QJsonObject custom, families;
    for (auto i = custom_.cbegin(); i != custom_.cend(); ++i)
        custom[QString::number(i.key())] = QJsonObject{
            {"name", names_[i.key()]}, {"pixels", QString::fromLatin1(i.value().toBase64())}};
    for (auto i = familyIcons_.cbegin(); i != familyIcons_.cend(); ++i)
        families[QString::number(i.key())] = i.value();
    return {{"version", 2}, {"icons", edits}, {"custom", custom}, {"families", families}};
}
void CategoryIcons::apply(const QJsonObject &object) {
    if (object["version"] != 2 || !object["icons"].isObject() || !object["custom"].isObject() ||
        !object["families"].isObject() || object.size() != 4)
        throw Error("Invalid class category artwork");
    auto candidate = *this;
    candidate.edits_.clear();
    candidate.custom_.clear();
    candidate.names_.clear();
    candidate.familyIcons_.clear();
    auto pixels = [](const QJsonValue &v) {
        const auto text = v.toString().toLatin1(),
                   data = QByteArray::fromBase64(text, QByteArray::AbortOnBase64DecodingErrors);
        if (!v.isString() || data.size() != 196 || data.toBase64() != text)
            throw Error("Invalid saved icon pixels");
        return data;
    };
    const auto custom = object["custom"].toObject();
    for (auto i = custom.begin(); i != custom.end(); ++i) {
        bool ok;
        const int id = i.key().toInt(&ok);
        const auto row = i.value().toObject();
        const auto name = row["name"].toString();
        if (!ok || QString::number(id) != i.key() || id < 9 || id > 255 || row.size() != 2 ||
            name.trimmed() != name || name.isEmpty() || name.size() > 80)
            throw Error("Invalid custom icon");
        candidate.custom_[id] = pixels(row["pixels"]);
        candidate.names_[id] = name;
    }
    const auto edits = object["icons"].toObject();
    for (auto i = edits.begin(); i != edits.end(); ++i) {
        bool ok;
        const int id = i.key().toInt(&ok);
        if (!ok || QString::number(id) != i.key())
            throw Error("Invalid category ID");
        checkId(id);
        candidate.setIndices(id, pixels(i.value()));
    }
    const auto families = object["families"].toObject();
    for (auto i = families.begin(); i != families.end(); ++i) {
        bool ok;
        const int id = i.key().toInt(&ok);
        const int icon = i.value().toInt(-1);
        if (!ok || QString::number(id) != i.key() || i.value().toDouble() != icon || icon < 1)
            throw Error("Invalid family icon assignment");
        candidate.assignFamily(id, icon);
    }
    *this = std::move(candidate);
}
QByteArray CategoryIcons::decodedPack(const Promotion *promotion) const {
    auto pack = source_.first(23496);
    for (auto i = edits_.cbegin(); i != edits_.cend(); ++i)
        for (int y = 0; y < 14; ++y)
            pack.replace(pixelOffset + y * 112 + (i.key() - 1) * 14, 14, i.value().mid(y * 14, 14));
    if (custom_.isEmpty() && familyIcons_.isEmpty() && !hasTabRuntime() &&
        source_.size() == 23496 && savedClassTabs_ == QByteArray(256, 0))
        return pack;
    QByteArray extra(528, 0);
    extra.replace(0, 8, "LCCATTAB");
    auto mapping = savedClassTabs_;
    if (promotion) {
        validateFamilies(*promotion);
        mapping.fill(0);
        const auto assignments = promotion->families["assignments"].toArray();
        const auto families = familyIcons_.keys();
        for (int i = 1; i < assignments.size(); ++i) {
            const int at = families.indexOf(assignments[i].toInt());
            if (at >= 0)
                mapping[i] = char(at + 9);
        }
    }
    extra.replace(8, 256, mapping);
    // Each tab names an icon cell; cells have a 16-pixel stride for the N64 loader.
    const auto ids = iconIds();
    for (int tab = 1; tab <= 8; ++tab)
        extra[264 + tab] = char(tab - 1);
    int tab = 9;
    for (auto i = familyIcons_.cbegin(); i != familyIcons_.cend(); ++i)
        extra[264 + tab++] = char(ids.indexOf(i.value()));
    for (int id : ids) {
        const auto icon = indices(id);
        for (int y = 0; y < 14; ++y) {
            extra += icon.mid(y * 14, 14);
            extra += QByteArray(2, 0);
        }
    }
    put32(extra, 520, extra.size());
    auto catalog = json();
    catalog["icons"] = QJsonObject{};
    const auto metadata = QJsonDocument(catalog).toJson(QJsonDocument::Compact);
    put32(extra, 524, metadata.size());
    extra += metadata;
    return pack + extra;
}

QByteArray CategoryIcons::rebuild(const Rom &rom, const QByteArray &compiled) const {
    if (compiled.first(23496) != decodedPack().first(23496))
        throw Error("Compiled category pixels differ from the project");
    if (compiled == loadedPack_)
        return rom.bytes();
    if (u32(rom.bytes(), ownerHigh) != high_ || u32(rom.bytes(), ownerLow) != low_ ||
        rom.bytes().mid(descriptorOffset, descriptorSize) != descriptor_ ||
        rom.bytes().mid(resourceBase + key_, sourceContainer_.size()) != sourceContainer_)
        throw Error("Another export owner changed the category artwork source");
    if (!placement_ && descriptor_ != QByteArray(descriptorSize, char(-1)))
        throw Error("Class category artwork descriptor space is occupied");
    const auto encoded = encodeLzss(compiled);
    if (decodeLzss(encoded, compiled.size()) != compiled)
        throw Error("Category artwork compression failed");
    QByteArray payload(8, 0);
    put32(payload, 0, encoded.size() + 4);
    put32(payload, 4, compiled.size());
    payload += encoded;
    payload += QByteArray((16 - payload.size() % 16) % 16, char(-1));
    const bool tail =
        placement_ && quint64(placement_) + allocation_ == quint64(rom.bytes().size());
    const quint32 at = placement_ && (payload.size() <= allocation_ || tail)
                           ? placement_
                           : (rom.bytes().size() + 15) & ~qsizetype(15);
    if (at < 0x2800000 || quint64(at) + payload.size() > 0x4000000)
        throw Error("Category artwork exceeds cartridge capacity");
    auto bytes = rom.bytes();
    if (placement_)
        bytes.replace(placement_, allocation_, QByteArray(allocation_, char(-1)));
    if (tail && at == placement_)
        bytes.resize(at);
    if (quint64(bytes.size()) < quint64(at) + payload.size())
        bytes += QByteArray(at + payload.size() - bytes.size(), char(-1));
    bytes.replace(at, payload.size(), payload);
    const auto newKey = at - resourceBase;
    put32(bytes, ownerHigh, (high_ & 0xffff0000) | (newKey >> 16));
    put32(bytes, ownerLow, (low_ & 0xffff0000) | (newKey & 65535));
    QByteArray d(descriptorSize, 0);
    d.replace(0, 8, "LC05CATS");
    put32(d, 8, 1);
    put32(d, 12, at);
    put32(d, 16, payload.size());
    put32(d, 20, hasFamilyTabs() || hasTabRuntime() ? 1 : 0);
    d.replace(32, 32, QByteArray::fromHex(sha256(payload)));
    bytes.replace(descriptorOffset, descriptorSize, d);
    const auto reopened = load(Rom::fromBytes(bytes));
    if (reopened.source_ != compiled.first(23496) || reopened.palette_ != palette_)
        throw Error("Category artwork export readback failed");
    return bytes;
}
} // namespace lordly
