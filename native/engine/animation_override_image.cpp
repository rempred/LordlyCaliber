#include "engine/animation_override_image.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <tuple>
namespace lordly {
namespace {
bool digestMatches(const QByteArray &bytes, const QJsonObject &object, const QString &field) {
    return sha256(bytes) == object[field].toString().toLatin1();
}
bool filled(const QByteArray &bytes, int size, char value) {
    return bytes == QByteArray(size, value);
}
// Canonical retail operands, used only for owner identity checks. Export writes
// these sites from maintained assembly compiled by the user's local toolchain.
QByteArray retailSite(const QString &name) {
    QByteArray result;
    if (name == "hook") {
        result.resize(8);
        put32(result, 0, 0x3c02801d);
        put32(result, 4, 0x8c42e8c0);
    } else if (name == "return") {
        result.resize(8);
        put32(result, 0, 0x03e00008);
        put32(result, 4, 0);
    } else {
        constexpr quint32 carriers[] = {0x322200ff, 0x322200ff, 0x305000ff,
                                        0x02002821, 0x305200ff, 0x02402821};
        bool ok = false;
        const int index = name.mid(7).toInt(&ok);
        if (!name.startsWith("carrier") || !ok || index < 0 || index >= 6)
            throw Error("Invalid animation carrier source identity");
        result.resize(4);
        put32(result, 0, carriers[index]);
    }
    return result;
}
auto sortKey(const AnimationOverrideImage::Row &r) {
    return std::tuple(r[0], r[1], r[2], r[3] ? 0 : 1, r[7], r[4]);
}
} // namespace
QByteArray AnimationOverrideImage::source(const QString &name) {
    QFile file(":/game/art/" + name);
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing animation override source: " + name);
    return file.readAll();
}
QJsonObject AnimationOverrideImage::definition() {
    static const auto value = QJsonDocument::fromJson(source("animation-overrides.json")).object();
    if (value["version"] != 1 || value["versions"].toArray().size() != 3 ||
        value["sites"].toArray().size() != 8)
        throw Error("Invalid animation override source catalog");
    return value;
}
AnimationOverrideImage AnimationOverrideImage::inspect(const Rom &rom) {
    AnimationOverrideImage result;
    try {
        rom.requireSupported();
        if (rom.revision())
            throw Error("Animation overrides require US revision zero");
        const auto d = definition(), layout = d["layout"].toObject();
        const int bootAt = layout["bootstrapZ64"].toInt(),
                  bootSize = layout["bootstrapSize"].toInt();
        const int moduleAt = layout["moduleZ64"].toInt(), moduleSize = layout["moduleSize"].toInt();
        auto retain = [&](int at, int size) {
            if (at < 0 || size < 1 || qint64(at) + size > rom.bytes().size())
                throw Error("Animation override region exceeds the cartridge");
            auto bytes = rom.bytes().mid(at, size);
            result.regions_[at] = bytes;
            return bytes;
        };
        const auto boot = retain(bootAt, bootSize), module = retain(moduleAt, moduleSize);
        bool retailSites = true;
        for (const auto &value : d["sites"].toArray()) {
            const auto site = value.toObject();
            const auto bytes = retain(site["offset"].toInt(), site["size"].toInt());
            retailSites &= digestMatches(bytes, site, "retailSha256");
        }
        if (retailSites && filled(boot, bootSize, 0) && filled(module, moduleSize, char(-1)))
            return result;
        const int version = u16(module, 4), count = u16(module, 12);
        if (!module.startsWith("OBSO") || version < 1 || version > 3 || count > 128)
            throw Error("Animation override code or its allocation is unrecognized; existing bytes "
                        "will be preserved");
        const auto description = d["versions"].toArray()[version - 1].toObject();
        const int loadedBootSize = description["bootstrapSize"].toInt();
        if (loadedBootSize < 1 || loadedBootSize > bootSize ||
            !digestMatches(boot.left(loadedBootSize), description, "bootstrapSha256") ||
            !filled(boot.mid(loadedBootSize), bootSize - loadedBootSize, 0))
            throw Error("Animation override bootstrap is partial or modified");
        for (const auto &value : d["sites"].toArray()) {
            const auto site = value.toObject();
            const auto bytes = result.regions_[site["offset"].toInt()];
            const bool matches =
                site["name"] == "hook"
                    ? digestMatches(bytes, description, "hookSha256")
                    : digestMatches(bytes, site, version == 1 ? "retailSha256" : "activeSha256");
            if (!matches)
                throw Error("Animation override instructions contain mixed versions or another "
                            "owner's edits");
        }
        auto immutable = module;
        put16(immutable, 12, 0);
        immutable.replace(0x20, 32, QByteArray(32, 0));
        immutable.replace(0x400, 1024, QByteArray(1024, 0));
        if (!digestMatches(immutable, description, "moduleTemplateSha256"))
            throw Error("Animation override module differs from its declared source");
        const auto table = module.mid(0x400, 1024);
        if (QByteArray::fromHex(sha256(table)) != module.mid(0x20, 32))
            throw Error("Animation override table checksum is invalid");
        if (!filled(table.mid(count * 8), 1024 - count * 8, 0))
            throw Error("Animation override table has undeclared records");
        QSet<QByteArray> identities;
        for (int i = 0; i < count; ++i) {
            Row row{};
            for (int j = 0; j < 8; ++j)
                row[j] = u8(table, i * 8 + j);
            if (row[3] > (version == 3 ? 3 : 1) || (!(row[3] & 1) && row[4]) || row[7] > 3 ||
                (!(row[3] & 2) && row[7]) || (version == 1 && (row[6] || row[7])) ||
                (version == 2 && row[7]))
                throw Error("Malformed animation override record");
            if (version == 1)
                row[6] = row[5];
            QByteArray identity;
            for (int field : {0, 1, 2, 3, 4, 7})
                identity += char(row[field]);
            if (identities.contains(identity))
                throw Error("Duplicate animation override record");
            identities.insert(identity);
            if (!result.rows_.isEmpty() && sortKey(row) < sortKey(result.rows_.last()))
                throw Error("Animation override records have ambiguous ordering");
            result.rows_ << row;
        }
        result.version_ = version;
    } catch (const Error &e) {
        result.reason_ = QString::fromUtf8(e.what());
        result.rows_.clear();
    }
    return result;
}
QByteArray AnimationOverrideImage::identity() const {
    QByteArray bytes;
    for (auto it = regions_.cbegin(); it != regions_.cend(); ++it) {
        QByteArray header(8, 0);
        put32(header, 0, quint32(it.key()));
        put32(header, 4, quint32(it->size()));
        bytes += header;
        bytes += *it;
    }
    return sha256(bytes);
}
void AnimationOverrideImage::requireUnchanged(const Rom &rom) const {
    if (!recognized())
        throw Error(reason_);
    for (auto it = regions_.cbegin(); it != regions_.cend(); ++it)
        if (rom.bytes().mid(it.key(), it->size()) != *it)
            throw Error("Another export owner changed an animation override region");
}
QByteArray AnimationOverrideImage::canonicalOwner(QByteArray bytes, qsizetype start) {
    for (const auto &value : definition()["sites"].toArray()) {
        const auto site = value.toObject();
        const int at = site["offset"].toInt(), size = site["size"].toInt();
        if (at < start || at + size > start + bytes.size())
            continue;
        const auto current = bytes.mid(at - start, size);
        if (digestMatches(current, site, "retailSha256"))
            continue;
        bool owned = digestMatches(current, site, "activeSha256");
        if (site["name"] == "hook")
            for (const auto &version : definition()["versions"].toArray())
                owned |= digestMatches(current, version.toObject(), "hookSha256");
        if (!owned)
            throw Error("Unrecognized animation override owner instruction");
        const auto original = retailSite(site["name"].toString());
        if (!digestMatches(original, site, "retailSha256"))
            throw Error("Animation override source identity is inconsistent");
        bytes.replace(at - start, size, original);
    }
    return bytes;
}
void AnimationOverrideImage::validateOwner(const Rom &rom, qsizetype start, qsizetype size) {
    if (rom.revision())
        return;
    for (const auto &value : definition()["sites"].toArray()) {
        const auto site = value.toObject();
        const int at = site["offset"].toInt(), length = site["size"].toInt();
        if (at < start || at + length > start + size ||
            digestMatches(rom.bytes().mid(at, length), site, "retailSha256"))
            continue;
        const auto result = inspect(rom);
        if (!result.recognized())
            throw Error(result.reason());
        return;
    }
}
} // namespace lordly
