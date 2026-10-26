#include "engine/weapon_types.h"
#include "engine/class_data.h"
#include "engine/item_data.h"
#include "engine/resource.h"
#include "engine/text.h"
#include <QJsonDocument>

namespace lordly {
QVector<WeaponTypeWord> weaponTypeTextWords(const Rom &rom) {
    const auto placement = ClassPlacement::read(rom);
    if (!placement || !placement->layout.items) return {};
    const auto data = ItemData::load(rom);
    if (data.weaponTypes().empty()) return {};
    const auto metadataRom = data.loadedTypesOffset();
    const auto metadataAddress = placement->layout.indexBase + quint32(metadataRom - placement->dataRomOffset);
    const auto labelAddress = metadataAddress + WeaponTypes::labelOffset;
    struct Pair { quint32 high, low, hi, lo; };
    const Pair pairs[] = {{0x94508, 0x94510, 0x3c10801f, 0x8e10e404},
        {0x95a3c, 0x95a40, 0x3c18801f, 0x2718e404}, {0x95a6c, 0x95a70, 0x3c18801f, 0x2718e404},
        {0x95a94, 0x95a98, 0x3c18801f, 0x2718e404}, {0x95b0c, 0x95b10, 0x3c18801f, 0x2718e404},
        {0x199654, 0x19965c, 0x3c108021, 0x8e1030a0}};
    QVector<WeaponTypeWord> result;
    for (const auto &pair : pairs) {
        result << WeaponTypeWord{pair.high, pair.hi, (pair.hi & 0xffff0000) | ((labelAddress + 0x8000) >> 16)};
        result << WeaponTypeWord{pair.low, pair.lo, (pair.lo & 0xffff0000) | (labelAddress & 65535)};
    }
    const auto key = quint32(metadataRom) + u32(rom.bytes(), metadataRom + 20) - WeaponTypes::helpKeyBase;
    result << WeaponTypeWord{0x563a0, 0x3c0501de, 0x3c050000 | (key >> 16)};
    result << WeaponTypeWord{0x563a4, 0x34a539da, 0x34a50000 | (key & 65535)};
    return result;
}
QByteArray canonicalWeaponTypeText(const Rom &rom, const QByteArray &owner, qsizetype start) {
    auto bytes = owner;
    for (const auto &word : weaponTypeTextWords(rom))
        if (word.offset >= start && word.offset + 4 <= start + bytes.size()) {
            const auto value = u32(bytes, word.offset - start);
            if (value != word.original && value != word.installed)
                throw Error("Equipment category text reference changed outside its rebuild");
            put32(bytes, word.offset - start, word.original);
        }
    return bytes;
}
WeaponTypes WeaponTypes::load(const Rom &rom, qsizetype metadata, qsizetype limit) {
    WeaponTypes result;
    if (rom.revision()) return result;
    // Army's retail category-name pointers and strings share its verified delta.
    for (int id = 0; id < 26; ++id) {
        const auto pointer = u32(rom.bytes(), 0xdae64 + id * 4);
        const qint64 at = qint64(pointer) - 0x801135a0;
        if (at < 0xdab00 || at >= 0xdae64) throw Error("Unrecognized equipment category names");
        const auto end = rom.bytes().indexOf(char(0), at);
        if (end < at || end >= 0xdae64 || end - at > 128)
            throw Error("Unterminated equipment category name");
        result.retailLabels_ << rom.bytes().mid(at, end - at);
    }
    const auto allocation = quint64(u32(rom.bytes(), retailHelpOffset)) + 4;
    if (allocation > 65536) throw Error("Equipment category help exceeds its allocation");
    result.retailHelp_ = Resource::load(rom, retailHelpOffset, allocation).decoded;
    if (metadata && rom.bytes().mid(metadata, 8) == "LCWTYPE1") {
        const auto total = u32(rom.bytes(), metadata + 12);
        const auto jsonAt = u32(rom.bytes(), metadata + 24), jsonSize = u32(rom.bytes(), metadata + 28);
        const auto helpAt = u32(rom.bytes(), metadata + 20);
        if (u32(rom.bytes(), metadata + 8) != 1 || total < groupOffset + 256 ||
            total > 131072 || metadata > limit || total > quint64(limit - metadata) ||
            u32(rom.bytes(), metadata + 16) != labelOffset ||
            helpAt < groupOffset + 256 || helpAt > total - 8 || jsonAt < helpAt + 8 ||
            jsonAt > total || jsonSize > total - jsonAt || jsonSize > 65536)
            throw Error("Invalid custom weapon type allocation");
        const auto placement = ClassPlacement::read(rom);
        if (!placement) throw Error("Weapon type data has no resident allocation");
        const auto address = placement->layout.indexBase + quint32(metadata - placement->dataRomOffset);
        for (int id = 0; id < 256; ++id) {
            const auto pointer = u32(rom.bytes(), metadata + labelOffset + id * 4);
            if (pointer < address + groupOffset + 256 || pointer >= address + helpAt)
                throw Error("Weapon type name lies outside its allocation");
            const auto at = metadata + pointer - address;
            const auto stop = rom.bytes().indexOf(char(0), at);
            if (stop < at || stop >= metadata + helpAt || stop - at > 128)
                throw Error("Unterminated weapon type name");
        }
        if (u32(rom.bytes(), metadata + helpAt) < 4 ||
            quint64(u32(rom.bytes(), metadata + helpAt)) + 4 > jsonAt - helpAt ||
            u32(rom.bytes(), metadata + helpAt + 4) > 65536)
            throw Error("Weapon type help lies outside its allocation");
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(rom.bytes().mid(metadata + jsonAt, jsonSize), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject())
            throw Error("Invalid custom weapon type metadata");
        const auto object = document.object();
        if (!object["names"].isObject() || !object["originalOwners"].isObject())
            throw Error("Incomplete custom weapon type metadata");
        result.setNames(object["names"].toObject());
        const auto owners = object["originalOwners"].toObject();
        for (auto it = owners.begin(); it != owners.end(); ++it) {
            bool ok;
            const auto start = it.key().toUInt(&ok);
            const auto hex = it->toString().toLatin1(), bytes = QByteArray::fromHex(hex);
            if (!ok || !it->isString() || bytes.toHex() != hex)
                throw Error("Invalid weapon type restoration data");
            result.originalOwners_[start] = bytes;
        }
    } else {
        const auto placement = ClassPlacement::read(rom);
        for (const auto [at, size] : {qMakePair(0x458f0U, 0x44), qMakePair(0x45934U, 0x11c),
                                    qMakePair(0x19b1b0U, 0x6c), qMakePair(0x19b800U, 0x2b4)}) {
            auto bytes = rom.bytes().mid(at, size);
            if (placement) bytes = placement->canonicalReferences(bytes, at);
            result.originalOwners_[at] = bytes;
        }
    }
    return result;
}
void WeaponTypes::setNames(const QJsonObject &names) {
    if (names.size() > maximumCustom) throw Error("All 230 custom weapon type slots are in use");
    QSet<QString> unique;
    for (auto it = names.begin(); it != names.end(); ++it) {
        bool ok;
        const int id = it.key().toInt(&ok);
        const auto name = it->toString();
        if (!ok || it.key() != QString::number(id) || !customId(id) || !it->isString() ||
            name.isEmpty() || name != name.trimmed() || name.size() > 32 || unique.contains(name.toCaseFolded()))
            throw Error("Custom weapon types need unique names of 1–32 characters and an available type ID");
        for (auto ch : name)
            if (ch.unicode() < 32 || ch.unicode() > 126)
                throw Error("Use printable game characters for weapon type names");
        if (encodeText(name, false) != name.toLatin1())
            throw Error("Weapon type names cannot contain game text commands");
        unique.insert(name.toCaseFolded());
    }
    names_ = names;
}
int WeaponTypes::create(const QString &name, const QSet<int> &used) {
    for (int id = 1; id < 255; ++id)
        if (customId(id) && !used.contains(id) && !names_.contains(QString::number(id))) {
            auto next = names_;
            next[QString::number(id)] = name.trimmed();
            setNames(next);
            return id;
        }
    throw Error("All custom weapon type IDs are already assigned");
}
QByteArray WeaponTypes::build(quint32 address) const {
    if (empty()) return {};
    if (retailLabels_.size() != 26) throw Error("Missing original equipment category labels");
    QByteArray bytes(groupOffset + 256, 0);
    bytes.replace(0, 8, "LCWTYPE1");
    put32(bytes, 8, 1);
    put32(bytes, 16, labelOffset);
    for (int id = 0; id < 256; ++id) {
        const auto label = names_.contains(QString::number(id)) ? names_[QString::number(id)].toString().toLatin1()
                          : id < 26 ? retailLabels_[id] : QByteArray("Weapon");
        put32(bytes, labelOffset + id * 4, address + quint32(bytes.size()));
        bytes += label + char(0);
        const int group = isWeapon(id) ? 1 : id == 23 ? 2 : (id == 14 || id == 15) ? 3
                         : (id >= 16 && id <= 20) ? 4 : id == 25 ? 5 : 0;
        put8(bytes, groupOffset + id, quint8(group));
    }
    QVector<QByteArray> help;
    for (qsizetype at = 0; at < retailHelp_.size();) {
        const auto end = retailHelp_.indexOf("\r\n", at);
        if (end < at) throw Error("Unterminated equipment category help record");
        help << retailHelp_.mid(at, end - at);
        at = end + 2;
    }
    if (help.size() < 102) throw Error("Equipment category help has an unexpected layout");
    while (help.size() < 331) help << QByteArray();
    for (auto it = names_.begin(); it != names_.end(); ++it)
        help[76 + it.key().toInt()] = encodeText(it->toString() + "\nWeapons for classes whose\ndefault weapon uses this type.", true);
    QByteArray decoded;
    for (const auto &entry : help) decoded += entry + "\r\n";
    auto encoded = encodeLzss(decoded);
    while (bytes.size() % 16) bytes += char(0);
    put32(bytes, 20, quint32(bytes.size()));
    QByteArray container(8, 0);
    put32(container, 0, quint32(encoded.size() + 4));
    put32(container, 4, quint32(decoded.size()));
    bytes += container + encoded;
    QJsonObject owners;
    for (auto it = originalOwners_.begin(); it != originalOwners_.end(); ++it)
        owners[QString::number(it.key())] = QString::fromLatin1(it->toHex());
    const auto metadata = QJsonDocument(QJsonObject{{"names", names_}, {"originalOwners", owners}}).toJson(QJsonDocument::Compact);
    put32(bytes, 24, quint32(bytes.size()));
    put32(bytes, 28, quint32(metadata.size()));
    bytes += metadata;
    while (bytes.size() % 16) bytes += char(0);
    put32(bytes, 12, quint32(bytes.size()));
    return bytes;
}
} // namespace lordly
