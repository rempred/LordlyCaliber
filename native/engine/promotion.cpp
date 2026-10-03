#include "engine/promotion.h"
#include "engine/class_data.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace lordly {
static constexpr int familyStart = 0x27d0000, familySize = 65536, magic = 0x4c43464d;
static bool filled(const QByteArray &bytes, char value) {
    return std::all_of(bytes.begin(), bytes.end(), [=](char c) { return c == value; });
}
quint32 crc32(const QByteArray &bytes) {
    quint32 crc = 0xffffffff;
    for (unsigned char byte : bytes) {
        crc ^= byte;
        for (int n = 0; n < 8; ++n)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
}
QJsonObject promotionOwners() {
    QFile file(":/game/promotion/owners.json");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing promotion source manifest");
    return QJsonDocument::fromJson(file.readAll()).object();
}
static int integer(const QJsonValue &v, int minimum, int maximum) {
    auto d = v.toDouble(-1);
    if (!v.isDouble() || !std::isfinite(d) || std::floor(d) != d || d < minimum || d > maximum)
        throw Error("Promotion value outside its supported range");
    return int(d);
}
QJsonObject Promotion::validateFamilies(const QJsonObject &value) {
    const auto version = value["schemaVersion"];
    const auto count = value["assignments"].toArray().size();
    if (!((version == 1 && count == 81) || (version == 2 && count >= 165 && count <= 256)) ||
        !value["families"].isArray() || !value["assignments"].isArray() || value.size() != 3)
        throw Error("Invalid class family catalog");
    QSet<int> ids;
    QSet<QString> names;
    auto list = value["families"].toArray();
    for (const auto &entry : list) {
        if (!entry.isObject())
            throw Error("Invalid class family");
        auto f = entry.toObject();
        auto id = integer(f["id"], 0, 65534), behavior = integer(f["behavior"], 0, 10);
        auto name = f["name"].toString();
        if (f.size() != 3 || ids.contains(id) || name.isEmpty() || name != name.trimmed() ||
            name.size() > 80 || names.contains(name.toCaseFolded()))
            throw Error("Families need unique IDs and names of 1–80 characters");
        for (auto c : name)
            if (c.unicode() < 32 || c.unicode() == 127)
                throw Error("Family names cannot contain control characters");
        if ((id <= 10 && behavior != id) || (id > 10 && behavior == 0) ||
            (id == 0 && name != "None"))
            throw Error("Retail family behaviors and the None family must remain intact");
        ids.insert(id);
        names.insert(name.toCaseFolded());
    }
    for (int id = 0; id <= 10; ++id)
        if (!ids.contains(id))
            throw Error("Missing retail family");
    auto assignments = value["assignments"].toArray();
    for (const auto &id : assignments)
        if (!ids.contains(integer(id, 0, 65534)))
            throw Error("Class references an undefined family");
    if (assignments[0] != 0)
        throw Error("The empty class must remain in family None");
    auto result = value;
    QVector<QJsonObject> sorted;
    for (auto f : list)
        sorted << f.toObject();
    std::sort(sorted.begin(), sorted.end(),
              [](const auto &a, const auto &b) { return a["id"].toInt() < b["id"].toInt(); });
    QJsonArray canonical;
    for (auto f : sorted)
        canonical << f;
    result["families"] = canonical;
    return result;
}
Promotion Promotion::load(const Rom &rom) {
    if (rom.revision() != 0)
        throw Error("Promotion source owners currently support US Rev 0");
    Promotion p;
    p.retailResource = Resource::load(rom, 0x23cdece, 384);
    p.gates = p.retailResource.decoded;
    if (p.gates.size() != 648)
        throw Error("Invalid promotion gate record count");
    QJsonArray list, assignments;
    for (int i = 0; i <= 10; ++i)
        list << QJsonObject{
            {"id", i}, {"name", i ? QString("Family %1").arg(i) : "None"}, {"behavior", i}};
    for (int i = 0; i < 81; ++i) {
        assignments << (u8(rom.bytes(), 0xdbd2c + i * 2) & 15);
        p.masks += char(u8(rom.bytes(), 0xdbd2d + i * 2));
        p.flags += char(u8(rom.bytes(), 0xdbd2c + i * 2) & 0xf0);
    }
    p.families = {{"schemaVersion", 1}, {"families", list}, {"assignments", assignments}};
    const auto slot = rom.bytes().mid(familyStart, familySize);
    const auto descriptor = slot.last(64);
    p.expanded = u32(descriptor, 0) == magic;
    const auto gateSlot = rom.bytes().mid(0x27b0800, 1024);
    const auto gateDescriptor = gateSlot.last(64);
    const bool gateDescriptorPresent = u32(gateDescriptor, 0) == 0x4f425347;
    if (gateDescriptorPresent && u8(gateDescriptor, 4) == 1)
        throw Error("This ROM uses legacy shared-DMA promotion redirects. Migrate it through "
                    "the browser Rebuild exporter before editing native promotion data");
    p.directGates = gateDescriptorPresent && u8(gateDescriptor, 4) == 2;
    if (gateDescriptorPresent && !p.directGates)
        throw Error("Unsupported promotion relocation descriptor version");
    if (p.expanded && p.directGates)
        throw Error("Conflicting promotion resource descriptors");
    bool hasFamilyCode = false;
    bool hasDirectGateCode = false;
    const auto classPlacement = ClassPlacement::read(rom);
    const auto owners = promotionOwners()["owners"].toArray();
    for (const auto &v : owners) {
        const auto owner = v.toObject();
        auto code = rom.bytes().mid(owner["start"].toInt(), owner["size"].toInt());
        if (classPlacement)
            code = classPlacement->canonicalReferences(code, owner["start"].toInt());
        auto hash = QString::fromLatin1(sha256(code));
        bool family =
            hash == owner["familySha256"].toString() ||
            (owner.contains("cSourceSha256") && hash == owner["cSourceSha256"].toString());
        bool direct =
            owner.contains("directGateSha256") && hash == owner["directGateSha256"].toString();
        hasFamilyCode |= family && hash != owner["retailSha256"].toString() &&
                         owner.value("requiresFamilyResource").toBool(true);
        hasDirectGateCode |= direct;
        if (p.expanded && !family)
            throw Error("Family resource has incomplete or modified runtime owners");
        if (!p.expanded && owner.contains("keySite") &&
            hash != owner[p.directGates ? "directGateSha256" : "retailSha256"].toString())
            throw Error("Promotion requirements have incomplete or modified resource loaders");
    }
    if (hasFamilyCode && !p.expanded)
        throw Error("Family runtime is missing its resource descriptor");
    if (hasDirectGateCode && !p.directGates)
        throw Error("Relocated promotion runtime is missing its resource descriptor");
    if (p.directGates) {
        auto logical = u32(gateDescriptor, 8), stored = u32(gateDescriptor, 12);
        if (u32(gateDescriptor, 4) != 0x02000100 || logical <= 376 || logical > 659 ||
            stored != ((logical + 1u) & ~1u) || u32(gateDescriptor, 16) != 648 ||
            u32(gateDescriptor, 24) != 0 ||
            crc32(gateDescriptor.first(28)) != u32(gateDescriptor, 28) ||
            !filled(gateDescriptor.mid(32), 0) || u32(gateSlot, 0) != stored + 4 ||
            u32(gateSlot, 4) != 648 || !filled(gateSlot.mid(8 + logical, stored - logical), 3) ||
            !filled(gateSlot.mid(8 + stored, 960 - 8 - stored), char(-1)))
            throw Error("Invalid relocated promotion descriptor or allocation");
        qsizetype consumed = 0;
        auto decoded = decodeLzss(gateSlot.mid(8, logical), 648, &consumed);
        if (consumed != logical || crc32(decoded) != u32(gateDescriptor, 20))
            throw Error("Relocated promotion requirements failed their integrity check");
        p.gates = decoded;
    }
    if (p.expanded) {
        auto logical = u32(descriptor, 8), size = u32(descriptor, 12),
             stored = (logical + 1u) & ~1u;
        if (u32(descriptor, 4) != 1 || logical == 0 || logical > 65464 || size < 980 ||
            size > 65536 || u32(slot, 0) != stored + 4 || u32(slot, 4) != size ||
            crc32(descriptor.first(20)) != u32(descriptor, 20) || !filled(descriptor.mid(24), 0))
            throw Error("Invalid family resource descriptor");
        qsizetype consumed = 0;
        auto decoded = decodeLzss(slot.mid(8, logical), size, &consumed);
        if (consumed != logical || crc32(decoded) != u32(descriptor, 16) ||
            !filled(slot.mid(8 + logical, stored - logical), 3) ||
            !filled(slot.mid(8 + stored, familySize - 64 - 8 - stored), char(-1)))
            throw Error("Family resource integrity check failed");
        if (!filled(rom.bytes().mid(0x27b0800, 1024), char(-1)))
            throw Error("Conflicting legacy promotion allocation");
        if (u32(decoded, 972) != magic || u32(decoded, 976) != size - 980)
            throw Error("Invalid family catalog header");
        p.families = validateFamilies(QJsonDocument::fromJson(decoded.mid(980)).object());
        p.gates = decoded.first(648);
        if (p.decodedFamilies().first(972) != decoded.first(972))
            throw Error("Family catalog and runtime table disagree");
    }
    p.families = validateFamilies(p.families);
    p.originalFamilies = p.families;
    p.originalGates = p.gates;
    p.originalMasks = p.masks;
    p.originalFlags = p.flags;
    return p;
}
void Promotion::setGate(int id, int field, int value) {
    if (id < 1 || id >= classCount() || field < 0 || field >= 8 || value < 0 || value > 255)
        throw Error("Promotion gate outside its supported range");
    gates[id * 8 + field] = char(value);
}
void Promotion::setMask(int id, int value) {
    if (id < 1 || id >= classCount() || value < 0 || value > 255)
        throw Error("Invalid compatibility mask");
    masks[id] = char(value);
}
void Promotion::setEquipmentSetListed(int id, bool listed) {
    if (id < 1 || id >= classCount())
        throw Error("Invalid class for the standard-equipment list");
    flags[id] = char((quint8(flags[id]) & 0x70) | (listed ? 0x80 : 0));
}
void Promotion::assign(int id, int familyId) {
    if (id < 1 || id >= classCount())
        throw Error("This class has no promotion entry yet");
    auto candidate = families;
    auto assignments = candidate["assignments"].toArray();
    assignments[id] = familyId;
    candidate["assignments"] = assignments;
    families = validateFamilies(candidate);
}
int Promotion::addFamily(const QString &name, int templateId) {
    auto candidate = families;
    auto list = candidate["families"].toArray();
    int next = 16, behavior = -1;
    for (auto f : list) {
        auto row = f.toObject();
        next = std::max(next, row["id"].toInt() + 1);
        if (row["id"].toInt() == templateId)
            behavior = row["behavior"].toInt();
    }
    if (behavior < 1)
        throw Error("Choose a numbered family as the new family's behavior template");
    list << QJsonObject{{"id", next}, {"name", name.trimmed()}, {"behavior", behavior}};
    candidate["families"] = list;
    families = validateFamilies(candidate);
    return next;
}
int Promotion::classCount() const {
    const auto count = families["assignments"].toArray().size();
    if (count != masks.size() || count != flags.size() || gates.size() != count * 8 ||
        !((families["schemaVersion"] == 1 && count == 81) ||
          (families["schemaVersion"] == 2 && count >= 165 && count <= 256)))
        throw Error("Promotion tables disagree on their class count");
    return int(count);
}
void Promotion::growToClassCount(int count) {
    const auto previous = classCount();
    if (count < 165 || count > 256 || count < previous)
        throw Error("Expanded promotion tables require 165–256 class IDs and cannot shrink");
    auto assignments = families["assignments"].toArray();
    while (assignments.size() < count)
        assignments.append(0);
    auto catalog = families;
    catalog["schemaVersion"] = 2;
    catalog["assignments"] = assignments;
    catalog = validateFamilies(catalog);
    // Commit together only after validation. Existing named/special classes
    // acquire no manual-promotion family merely because the table grows.
    gates.append(QByteArray((count - previous) * 8, 0));
    masks.append(QByteArray(count - previous, 0));
    flags.append(QByteArray(count - previous, 0));
    families = catalog;
}
int Promotion::appendClass(int templateId) {
    auto candidate = *this;
    if (candidate.classCount() == 81)
        candidate.growToClassCount(165);
    const auto id = candidate.classCount();
    if (templateId < 0 || templateId >= id || id == 256)
        throw Error("Invalid class template or no byte-sized class IDs remain");
    candidate.growToClassCount(id + 1);
    candidate.gates.replace(id * 8, 8, candidate.gates.mid(templateId * 8, 8));
    candidate.masks[id] = candidate.masks[templateId];
    candidate.flags[id] = candidate.flags[templateId];
    auto assignments = candidate.families["assignments"].toArray();
    assignments[id] = assignments[templateId];
    candidate.families["assignments"] = assignments;
    *this = std::move(candidate);
    return id;
}
PromotionData Promotion::buildExpandedData() const {
    const auto count = classCount();
    if (count < 165)
        throw Error("The retail promotion model uses its existing resource layout");
    const auto catalog = validateFamilies(families);
    PromotionData result{count, count * 8, count * 12, count * 13, (count * 14 + 3) & ~3, gates};
    QMap<int, int> behaviors;
    for (const auto &value : catalog["families"].toArray()) {
        const auto row = value.toObject();
        behaviors[row["id"].toInt()] = row["behavior"].toInt();
    }
    result.bytes.resize(result.maskOffset);
    const auto assignments = catalog["assignments"].toArray();
    for (int id = 0; id < count; ++id) {
        const auto family = assignments[id].toInt();
        put32(result.bytes, result.familyOffset + id * 4,
              quint32(behaviors[family] * 65536 + family));
    }
    result.bytes += masks;
    for (auto value : flags)
        if (quint8(value) & 15)
            throw Error("Class-line flags must not contain legacy family bits");
    result.bytes += flags;
    result.bytes.append(QByteArray(result.catalogOffset - result.bytes.size(), 0));
    const auto json = QJsonDocument(catalog).toJson(QJsonDocument::Compact);
    result.bytes.resize(result.catalogOffset + 8);
    put32(result.bytes, result.catalogOffset, magic);
    put32(result.bytes, result.catalogOffset + 4, json.size());
    result.bytes += json;
    if (result.bytes.size() > 65536)
        throw Error("Expanded promotion data exceeds the game's resource limit");
    return result;
}
void Promotion::applyExpandedData(const QByteArray &bytes, int count) {
    if (count < 165 || count > 256 || bytes.size() > 65536)
        throw Error("Invalid expanded promotion record count or size");
    const auto catalogOffset = (count * 14 + 3) & ~3;
    if (bytes.size() < catalogOffset + 8 || u32(bytes, catalogOffset) != magic ||
        u32(bytes, catalogOffset + 4) != quint32(bytes.size() - catalogOffset - 8) ||
        !filled(bytes.mid(count * 14, catalogOffset - count * 14), 0))
        throw Error("Invalid expanded promotion catalog framing");
    const auto catalog =
        validateFamilies(QJsonDocument::fromJson(bytes.mid(catalogOffset + 8)).object());
    if (catalog["schemaVersion"] != 2 || catalog["assignments"].toArray().size() != count)
        throw Error("Expanded promotion catalog has a different record count");
    auto candidate = *this;
    candidate.apply({{"families", catalog},
                     {"gates", QString::fromLatin1(bytes.first(count * 8).toHex())},
                     {"masks", QString::fromLatin1(bytes.mid(count * 12, count).toHex())},
                     {"flags", QString::fromLatin1(bytes.mid(count * 13, count).toHex())}});
    if (candidate.buildExpandedData().bytes.first(catalogOffset) != bytes.first(catalogOffset))
        throw Error("Expanded promotion catalog and runtime family words disagree");
    *this = std::move(candidate);
}
QJsonObject Promotion::json() const {
    return {{"families", families},
            {"gates", QString::fromLatin1(gates.toHex())},
            {"masks", QString::fromLatin1(masks.toHex())},
            {"flags", QString::fromLatin1(flags.toHex())}};
}
void Promotion::apply(const QJsonObject &value) {
    auto f = validateFamilies(value["families"].toObject());
    auto g = QByteArray::fromHex(value["gates"].toString().toLatin1());
    auto m = QByteArray::fromHex(value["masks"].toString().toLatin1());
    const auto count = f["assignments"].toArray().size();
    auto bits = value.contains("flags") ? QByteArray::fromHex(value["flags"].toString().toLatin1())
                                        : originalFlags;
    if (g.size() != count * 8 || m.size() != count ||
        value["gates"].toString().toLatin1().toLower() != g.toHex() ||
        value["masks"].toString().toLatin1().toLower() != m.toHex() ||
        value.size() != (value.contains("flags") ? 4 : 3) || bits.size() != count ||
        (value.contains("flags") &&
         value["flags"].toString().toLatin1().toLower() != bits.toHex()) ||
        std::any_of(bits.begin(), bits.end(), [](char v) { return quint8(v) & 15; }) ||
        g.first(8) != originalGates.first(8) || m.first(1) != originalMasks.first(1) ||
        bits.first(1) != originalFlags.first(1))
        throw Error("Invalid saved promotion data");
    families = f;
    gates = g;
    masks = m;
    flags = bits;
}
bool Promotion::modified() const {
    return families != originalFamilies || gates != originalGates || masks != originalMasks ||
           flags != originalFlags;
}
QByteArray Promotion::decodedFamilies() const {
    if (classCount() != 81)
        throw Error("Expanded promotion tables require the expanded runtime layout");
    auto out = gates;
    QMap<int, int> behaviors;
    for (auto f : families["families"].toArray())
        behaviors[f.toObject()["id"].toInt()] = f.toObject()["behavior"].toInt();
    for (auto id : families["assignments"].toArray()) {
        auto pos = out.size();
        out.resize(pos + 4);
        put32(out, pos, quint32(behaviors[id.toInt()] * 65536 + id.toInt()));
    }
    auto json = QJsonDocument(families).toJson(QJsonDocument::Compact);
    out.resize(980);
    put32(out, 972, magic);
    put32(out, 976, json.size());
    out += json;
    if (out.size() > 65536)
        throw Error("Family catalog exceeds the game's resource limit");
    return out;
}
QByteArray Promotion::familyContainer(const QByteArray &compiled) const {
    if (classCount() != 81)
        throw Error("Expanded promotion data requires a versioned runtime descriptor");
    auto stream = encodeLzss(compiled);
    auto stored = (stream.size() + 1) & ~qsizetype(1);
    if (stored + 8 > familySize - 64)
        throw Error("Family resource exceeds its ROM allocation");
    QByteArray out(familySize, char(-1));
    put32(out, 0, stored + 4);
    put32(out, 4, compiled.size());
    out.replace(8, stored, QByteArray(stored, 3));
    out.replace(8, stream.size(), stream);
    QByteArray descriptor(64, 0);
    put32(descriptor, 0, magic);
    put32(descriptor, 4, 1);
    put32(descriptor, 8, stream.size());
    put32(descriptor, 12, compiled.size());
    put32(descriptor, 16, crc32(compiled));
    put32(descriptor, 20, crc32(descriptor.first(20)));
    out.replace(familySize - 64, 64, descriptor);
    return out;
}
} // namespace lordly
