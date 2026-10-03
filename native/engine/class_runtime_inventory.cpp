#include "engine/class_runtime_inventory.h"
#include "engine/promotion.h"
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace lordly {
QJsonArray expandedPromotionOwners() {
    const auto catalog = promotionOwners();
    auto result = catalog["owners"].toArray();
    for (const auto &owner : catalog["expandedOwners"].toArray())
        result.append(owner);
    return result;
}
QMap<QString, quint32> ClassRuntimeInventory::canonicalBases() {
    return {{"classes", 0x80187bcc},
            {"appearances", 0x8018ece0},
            {"aliases", 0x801ef3b8},
            {"discovery", 0x80196a76}};
}
QMap<QString, quint32> ClassRuntimeInventory::dataBases(const ClassDataLayout &layout) {
    return {{"classes", layout.indexBase},
            {"appearances", layout.appearances},
            {"aliases", layout.aliases},
            {"discovery", layout.discovery}};
}
static void validateRecord(const RuntimeOwnerRecord &record) {
    bool known = false;
    for (const auto &entry : expandedPromotionOwners()) {
        const auto owner = entry.toObject();
        known |= record.start == quint32(owner["start"].toInt()) &&
                 record.size == quint32(owner["size"].toInt());
    }
    if (!known || record.references.size() > 256 ||
        !QRegularExpression("^[0-9a-f]{64}$")
             .match(QString::fromLatin1(record.canonicalSha256))
             .hasMatch())
        throw Error("Invalid or unowned class runtime inventory entry");
    const auto groups = ClassRuntimeInventory::canonicalBases();
    QSet<QString> seen;
    for (const auto &ref : record.references) {
        const auto key = QString("%1/%2").arg(ref.high).arg(ref.low);
        if (!groups.contains(ref.group) || ref.high == ref.low || ((ref.high | ref.low) & 3) ||
            quint64(ref.high) + 4 > record.size || quint64(ref.low) + 4 > record.size ||
            seen.contains(key))
            throw Error("Invalid class runtime address reference");
        seen.insert(key);
    }
}
static quint32 address(const RuntimeDataReference &ref, const QMap<QString, quint32> &bases) {
    if (!bases.contains(ref.group))
        throw Error("Missing class runtime data placement");
    const qint64 result = qint64(bases[ref.group]) + ref.addend;
    if (result < 0x80000000LL || result >= 0x80400000LL)
        throw Error("Class runtime reference leaves the 4 MB address range");
    return quint32(result);
}
static QByteArray transform(const RuntimeOwnerRecord &record, const QByteArray &code,
                            const QMap<QString, quint32> &from, const QMap<QString, quint32> &to) {
    if (code.size() != record.size)
        throw Error("Class runtime owner has a different extent");
    auto result = code;
    QMap<quint32, quint32> writes;
    auto put = [&](quint32 site, quint32 word) {
        if (writes.contains(site) && writes[site] != word)
            throw Error("Conflicting class runtime address references");
        writes[site] = word;
        put32(result, site, word);
    };
    for (const auto &ref : record.references) {
        const auto high = u32(code, ref.high), low = u32(code, ref.low);
        const quint32 carry = low >> 26 == 13 ? 0 : 0x8000;
        const auto before = address(ref, from), after = address(ref, to);
        if (high >> 26 != 15 || (high & 65535) != (before + carry) >> 16 ||
            (low & 65535) != (before & 65535))
            throw Error("Class runtime code disagrees with its declared data placement");
        put(ref.high, (high & 0xffff0000U) | ((after + carry) >> 16));
        put(ref.low, (low & 0xffff0000U) | (after & 65535));
    }
    return result;
}
RuntimeOwnerRecord ClassRuntimeInventory::capture(quint32 start, const QByteArray &code,
                                                  const QVector<RuntimeDataReference> &references,
                                                  const QMap<QString, quint32> &bases) {
    RuntimeOwnerRecord result{start, quint32(code.size()), QByteArray(64, '0'), references};
    validateRecord(result);
    result.canonicalSha256 = sha256(transform(result, code, bases, canonicalBases()));
    return result;
}
QByteArray ClassRuntimeInventory::encode() const {
    if (classCount < 165 || classCount > 256 || owners.isEmpty() || owners.size() > 64)
        throw Error("Invalid class runtime inventory count");
    auto sorted = owners;
    std::sort(sorted.begin(), sorted.end(),
              [](const auto &a, const auto &b) { return a.start < b.start; });
    QJsonArray entries;
    quint64 end = 0;
    for (const auto &owner : sorted) {
        validateRecord(owner);
        if (owner.start < end)
            throw Error("Overlapping class runtime inventory owners");
        end = quint64(owner.start) + owner.size;
        QJsonArray references;
        for (const auto &ref : owner.references)
            references << QJsonObject{{"group", ref.group},
                                      {"high", double(ref.high)},
                                      {"low", double(ref.low)},
                                      {"addend", ref.addend}};
        entries << QJsonObject{{"start", double(owner.start)},
                               {"size", double(owner.size)},
                               {"sha256", QString::fromLatin1(owner.canonicalSha256)},
                               {"references", references}};
    }
    const auto payload = QJsonDocument(QJsonObject{{"classCount", classCount}, {"owners", entries}})
                             .toJson(QJsonDocument::Compact);
    if (payload.size() > 65536 - 24)
        throw Error("Class runtime inventory exceeds its ROM metadata limit");
    QByteArray bytes(24, 0);
    bytes.replace(0, 8, "LC05OWNR");
    put32(bytes, 8, 1);
    put32(bytes, 12, payload.size());
    put32(bytes, 16, crc32(payload));
    put32(bytes, 20, crc32(bytes.first(20)));
    return bytes + payload;
}
ClassRuntimeInventory ClassRuntimeInventory::decode(const QByteArray &bytes) {
    if (bytes.size() < 24 || bytes.size() > 65536 || !bytes.startsWith("LC05OWNR") ||
        u32(bytes, 8) != 1 || u32(bytes, 12) != bytes.size() - 24 ||
        u32(bytes, 16) != crc32(bytes.sliced(24)) || u32(bytes, 20) != crc32(bytes.first(20)))
        throw Error("Invalid or damaged class runtime inventory");
    auto number = [](const QJsonValue &v, qint64 min, qint64 max) -> qint64 {
        const auto n = v.toDouble();
        if (!v.isDouble() || !std::isfinite(n) || std::floor(n) != n || n < min || n > max)
            throw Error("Class runtime inventory integer is out of range");
        return qint64(n);
    };
    const auto json = QJsonDocument::fromJson(bytes.sliced(24)).object();
    if (json.size() != 2 || !json["owners"].isArray())
        throw Error("Invalid class runtime inventory structure");
    ClassRuntimeInventory result;
    result.classCount = int(number(json["classCount"], 165, 256));
    for (const auto &entry : json["owners"].toArray()) {
        const auto owner = entry.toObject();
        if (owner.size() != 4 || !owner["sha256"].isString() || !owner["references"].isArray())
            throw Error("Invalid class runtime owner structure");
        RuntimeOwnerRecord record;
        record.start = quint32(number(owner["start"], 0, 0x3ffffff));
        record.size = quint32(number(owner["size"], 4, 0x10000));
        record.canonicalSha256 = owner["sha256"].toString().toLatin1();
        for (const auto &value : owner["references"].toArray()) {
            const auto ref = value.toObject();
            if (ref.size() != 4 || !ref["group"].isString())
                throw Error("Invalid class runtime reference structure");
            record.references.append({ref["group"].toString(),
                                      quint32(number(ref["high"], 0, record.size)),
                                      quint32(number(ref["low"], 0, record.size)),
                                      qint32(number(ref["addend"], -0x80000000LL, 0x7fffffff))});
        }
        result.owners.append(record);
    }
    result.encode(); // Reuse ownership, duplicate, extent and reference validation.
    return result;
}
QByteArray ClassRuntimeInventory::relocate(quint32 start, const QByteArray &code,
                                           const QMap<QString, quint32> &from,
                                           const QMap<QString, quint32> &to) const {
    for (const auto &owner : owners) {
        if (owner.start != start)
            continue;
        validateRecord(owner);
        const auto canonical = transform(owner, code, from, canonicalBases());
        if (sha256(canonical) != owner.canonicalSha256)
            throw Error("Class runtime code changed outside its recorded rebuild");
        return transform(owner, canonical, canonicalBases(), to);
    }
    throw Error("Class runtime owner is missing from its inventory");
}
void ClassRuntimeInventory::verify(const Rom &rom, const QMap<QString, quint32> &bases) const {
    encode(); // Require a valid, nonempty inventory before checking its code.
    for (const auto &owner : owners)
        relocate(owner.start, rom.bytes().mid(owner.start, owner.size), bases, bases);
}
} // namespace lordly
