#include "engine/world_resources.h"
#include "engine/promotion.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace lordly {
namespace {
QByteArray selectors(const Rom &rom) {
    QByteArray result;
    for (int key = 0; key < WorldResources::keyCount; ++key) {
        const auto at = WorldResources::selectorTable + key * WorldResources::selectorStride;
        result += char(u8(rom.bytes(), at + 6));
        result += char(u8(rom.bytes(), at + 5));
    }
    return result;
}
quint32 entry(const Rom &rom, int slot) {
    const auto key = u32(rom.bytes(), WorldResources::root + 4 + slot * 4);
    if (key >= 0x10000000 || key % 2)
        throw Error("World resource key is outside the original loader domain");
    return key + WorldResources::keyBase;
}
QVector<LhaMember> group(const Rom &rom, quint32 at) {
    const auto length = u32(rom.bytes(), at);
    if (!length || length > 0x400000 || quint64(at) + 4 + length > quint64(rom.bytes().size()))
        throw Error("World archive collection exceeds its envelope");
    const quint32 end = at + 4 + length;
    at += 4;
    QVector<LhaMember> result;
    while (at < end - 1) {
        const auto member = LhaMember::read(rom.bytes(), at);
        at += quint32(member.headerSize + member.storedSize);
        if (at > end - 1 || result.size() == 256)
            throw Error("World archive exceeds its envelope or unsigned selector");
        result << member;
    }
    if (at != end - 1 || u8(rom.bytes(), at) || result.isEmpty())
        throw Error("World archive collection has invalid termination");
    return result;
}
QByteArray stored(const Rom &rom, const LhaMember &member) {
    return rom.bytes().mid(member.offset, member.headerSize + member.storedSize);
}
QByteArray envelope(const QByteArray &data) {
    QByteArray result(4, 0);
    put32(result, 0, quint32(data.size()));
    return result + data;
}
void keyRange(int key) {
    if (key < 0 || key >= WorldResources::keyCount)
        throw Error("Scenario key is outside the world resource table");
}
} // namespace
WorldResources WorldResources::load(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision() != 0)
        throw Error("World resources currently require US revision 0");
    QFile owners(":/game/scenarios/world-owners.json");
    if (!owners.open(QIODevice::ReadOnly))
        throw Error("Missing world resource reader catalog");
    for (const auto &value :
         QJsonDocument::fromJson(owners.readAll()).object()["spans"].toArray()) {
        const auto span = value.toObject();
        if (sha256(rom.bytes().mid(span["start"].toInt(), span["size"].toInt())) !=
            span["sha256"].toString().toLatin1())
            throw Error(span["name"].toString() +
                        " differs from the supported world resource path");
    }
    if (u32(rom.bytes(), root) != 12)
        throw Error("World resource directory must contain three collection keys");
    WorldResources result;
    const auto te = entry(rom, 0), se = entry(rom, 2);
    result.treasure_ = group(rom, te);
    result.sites_ = group(rom, se);
    const auto maps = group(rom, entry(rom, 1));
    if (maps.size() != 3 || maps[0].name != "spmap46.bin" || maps[1].name != "spmap47.bin" ||
        maps[2].name != "spmap48.bin")
        throw Error("World resource directory has an unrecognized middle collection");
    if (result.treasure_.size() < 40 || result.sites_.size() < 61 ||
        result.sites_[0].name != "ktenmain.bin" || result.sites_[60].name != "shopcsv.bin")
        throw Error("World resource collection has lost its original member directory");
    for (const auto &m : result.treasure_)
        if (!m.name.startsWith("maizo") || !m.name.endsWith(".bin"))
            throw Error("Treasure collection contains an unexpected member");
    for (int i = 1; i < result.sites_.size(); ++i)
        if (i != 60 && (!result.sites_[i].name.startsWith("scincsv") ||
                        !result.sites_[i].name.endsWith(".bin")))
            throw Error("Site collection contains an unexpected member");
    result.selectors_ = selectors(rom);
    result.baseline_ = result.selectors_;
    const auto h = rom.bytes().mid(descriptorOffset, descriptorSize);
    if (h.startsWith("LC05WRLD")) {
        if (h.size() != descriptorSize || u32(h, 8) != 1 || h.mid(56, 36) != QByteArray(36, 0) ||
            crc32(h.first(92)) != u32(h, 92))
            throw Error("Damaged world resource descriptor");
        const WorldPlacement p{u32(h, 12), u32(h, 16)};
        if (p.offset < 0x2800000 || p.offset % 16 || !p.allocation || p.allocation % 16 ||
            quint64(p.offset) + p.allocation > 0x4000000 ||
            quint64(p.offset) + p.allocation > quint64(rom.bytes().size()))
            throw Error("World resource allocation exceeds the cartridge");
        const auto data = rom.bytes().mid(p.offset, p.allocation);
        const auto tn = u32(h, 20), so = u32(h, 24), sn = u32(h, 28), bo = u32(h, 32);
        if (tn < 4 || so != ((quint64(tn) + 15) & ~quint64(15)) || sn < 4 ||
            quint64(so) + sn != bo || quint64(bo) + keyCount * 2 > p.allocation || te != p.offset ||
            se != p.offset + so || u32(data, 0) + 4 != tn || u32(data, so) + 4 != sn ||
            crc32(data) != u32(h, 36) || crc32(result.selectors_) != u32(h, 40) ||
            u32(h, 44) != 40 || u32(h, 48) != 61 || u32(h, 52) != u32(rom.bytes(), root + 8))
            throw Error("World resources differ from their declared allocation");
        result.baseline_ = data.mid(bo, keyCount * 2);
        result.placement_ = p;
    } else if (result.treasure_.size() != 40 || result.sites_.size() != 61) {
        throw Error("Expanded world resources require an allocation descriptor");
    }
    for (int key = 0; key < keyCount; ++key) {
        if (result.treasureSelector(key) >= result.treasure_.size() ||
            result.siteSelector(key) >= result.sites_.size() || result.siteSelector(key) == 60 ||
            u8(result.baseline_, key * 2) >= 40 || u8(result.baseline_, key * 2 + 1) >= 60)
            throw Error("World selector points outside its resource collection");
    }
    QByteArray identity = rom.bytes().mid(root, 16) + result.selectors_ + h;
    for (const auto &list : {result.treasure_, result.sites_, maps})
        for (const auto &m : list) {
            m.decode(rom);
            identity += stored(rom, m);
        }
    result.identity_ = sha256(identity);
    return result;
}
int WorldResources::treasureSelector(int key) const {
    keyRange(key);
    return u8(selectors_, key * 2);
}
int WorldResources::siteSelector(int key) const {
    keyRange(key);
    return u8(selectors_, key * 2 + 1);
}
QByteArray WorldResources::treasure(const Rom &rom, int key) const {
    return treasure_[treasureSelector(key)].decode(rom);
}
QByteArray WorldResources::sites(const Rom &rom, int key) const {
    const int index = siteSelector(key);
    return index ? sites_[index].decode(rom) : QByteArray();
}
QByteArray WorldResources::rebuild(const Rom &rom, const QByteArray &townsData,
                                   const QMap<int, QByteArray> &treasureEdits,
                                   const QMap<int, QByteArray> &siteEdits) const {
    if (load(rom).identity_ != identity_)
        throw Error("World resources changed since they were loaded");
    if (townsData.size() != 316 * 28)
        throw Error("Site table must retain its 316 complete records");
    bool changed = townsData != towns(rom);
    for (int type = 0; type < 2; ++type) {
        const auto &edits = type ? siteEdits : treasureEdits;
        for (auto it = edits.begin(); it != edits.end(); ++it) {
            keyRange(it.key());
            if (!it.key() || it.value().isEmpty() || (type && !siteSelector(it.key())))
                throw Error("Choose a scenario with an existing world resource for editing");
            changed |= it.value() != (type ? sites(rom, it.key()) : treasure(rom, it.key()));
        }
    }
    if (!changed)
        return rom.bytes();
    if (!placement_ &&
        rom.bytes().mid(descriptorOffset, descriptorSize) != QByteArray(descriptorSize, char(-1)))
        throw Error("World descriptor space belongs to another modification");
    QByteArray nextSelectors = baseline_, collections[2];
    for (int type = 0; type < 2; ++type) {
        const auto &members = type ? sites_ : treasure_;
        const auto &edits = type ? siteEdits : treasureEdits;
        const int count = type ? 61 : 40;
        QVector<QByteArray> decoded;
        for (int i = 0; i < count; ++i) {
            decoded << members[i].decode(rom);
            collections[type] += type && i == 0 && townsData != decoded[i]
                                     ? members[i].rebuild(rom, townsData)
                                     : stored(rom, members[i]);
        }
        QMap<QPair<int, QByteArray>, int> added;
        for (int key = 0; key < keyCount; ++key) {
            const int original = u8(baseline_, key * 2 + type);
            if (type && !original)
                continue;
            const auto desired = edits.contains(key) ? edits[key]
                                 : type              ? sites(rom, key)
                                                     : treasure(rom, key);
            if (desired == decoded[original])
                continue;
            const QPair<int, QByteArray> identity{original, desired};
            if (!added.contains(identity)) {
                const int ordinal = count + int(added.size());
                if (ordinal > 255)
                    throw Error("World collection exceeds its unsigned member selector");
                added[identity] = ordinal;
                collections[type] += members[original].rebuild(rom, desired);
            }
            nextSelectors[key * 2 + type] = char(added[identity]);
        }
        collections[type] = envelope(collections[type] + char(0));
    }
    auto payload = collections[0];
    payload += QByteArray((16 - payload.size() % 16) % 16, char(-1));
    const auto siteOffset = quint32(payload.size());
    payload += collections[1];
    const auto baselineOffset = quint32(payload.size());
    payload += baseline_;
    payload += QByteArray((16 - payload.size() % 16) % 16, char(-1));
    const quint32 allocation = quint32(payload.size());
    const bool tail = placement_ && quint64(placement_->offset) + placement_->allocation ==
                                        quint64(rom.bytes().size());
    const quint32 offset = placement_ && (allocation <= placement_->allocation || tail)
                               ? placement_->offset
                               : quint32((rom.bytes().size() + 15) & ~qsizetype(15));
    if (offset < 0x2800000 || quint64(offset) + allocation > 0x4000000)
        throw Error("Rebuilt world resources exceed the cartridge");
    auto image = rom.bytes();
    if (placement_)
        image.replace(placement_->offset, placement_->allocation,
                      QByteArray(placement_->allocation, char(-1)));
    if (tail && offset == placement_->offset)
        image.resize(offset);
    if (quint64(image.size()) < quint64(offset) + allocation)
        image += QByteArray(offset + allocation - image.size(), char(-1));
    image.replace(offset, allocation, payload);
    put32(image, root + 4, offset - keyBase);
    put32(image, root + 12, offset + siteOffset - keyBase);
    for (int key = 0; key < keyCount; ++key) {
        put8(image, selectorTable + key * selectorStride + 6, u8(nextSelectors, key * 2));
        put8(image, selectorTable + key * selectorStride + 5, u8(nextSelectors, key * 2 + 1));
    }
    QByteArray descriptor(descriptorSize, 0);
    descriptor.replace(0, 8, "LC05WRLD");
    put32(descriptor, 8, 1);
    put32(descriptor, 12, offset);
    put32(descriptor, 16, allocation);
    put32(descriptor, 20, quint32(collections[0].size()));
    put32(descriptor, 24, siteOffset);
    put32(descriptor, 28, quint32(collections[1].size()));
    put32(descriptor, 32, baselineOffset);
    put32(descriptor, 36, crc32(payload));
    put32(descriptor, 40, crc32(nextSelectors));
    put32(descriptor, 44, 40);
    put32(descriptor, 48, 61);
    put32(descriptor, 52, u32(image, root + 8));
    put32(descriptor, 92, crc32(descriptor.first(92)));
    image.replace(descriptorOffset, descriptorSize, descriptor);
    const auto reopened = Rom::fromBytes(image);
    const auto check = load(reopened);
    if (check.towns(reopened) != townsData || check.selectors_ != nextSelectors ||
        check.shopInventory(reopened) != shopInventory(rom))
        throw Error("World resource rebuild readback failed");
    for (auto it = treasureEdits.begin(); it != treasureEdits.end(); ++it)
        if (check.treasure(reopened, it.key()) != it.value())
            throw Error("Rebuilt treasure differs from the authored data");
    for (auto it = siteEdits.begin(); it != siteEdits.end(); ++it)
        if (check.sites(reopened, it.key()) != it.value())
            throw Error("Rebuilt sites differ from the authored data");
    return image;
}
} // namespace lordly
