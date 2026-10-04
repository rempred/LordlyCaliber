#include "engine/scenario_resources.h"
#include "engine/eset.h"
#include "engine/promotion.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace lordly {
static QByteArray selectors(const Rom &rom) {
    QByteArray result;
    for (int key = 0; key < ScenarioResources::keyCount; ++key)
        result += char(u8(rom.bytes(), ScenarioResources::selectorTable +
                                           key * ScenarioResources::selectorStride + 8));
    return result;
}
static quint32 linkedEntry(const Rom &rom, quint32 link) {
    if (u32(rom.bytes(), link) != 4)
        throw Error("Scenario resource link no longer contains one key");
    const auto key = u32(rom.bytes(), link + 4);
    if (key >= 0x10000000 || key % 2)
        throw Error("Scenario resource key is outside the loader's domain");
    return key + ScenarioResources::keyBase;
}
static QVector<LhaMember> group(const Rom &rom, quint32 entry) {
    const auto &bytes = rom.bytes();
    const auto length = u32(bytes, entry);
    if (!length || length > 0x400000 || quint64(entry) + 4 + length > quint64(bytes.size()))
        throw Error("Scenario archive collection exceeds its resource envelope");
    const qsizetype end = qsizetype(entry) + 4 + length;
    qsizetype at = entry + 4;
    QVector<LhaMember> result;
    while (at < end - 1) {
        const auto member = LhaMember::read(bytes, at);
        at += member.headerSize + member.storedSize;
        if (at > end - 1 || result.size() == 128)
            throw Error("Scenario archive member exceeds its envelope or signed index");
        result << member;
    }
    if (at != end - 1 || u8(bytes, at) != 0 || result.isEmpty())
        throw Error("Scenario archive collection has invalid termination");
    return result;
}
static void enemySize(const QByteArray &data) {
    if (data.size() < 556 * 35 || data.size() % 35 || data.size() / 35 > 65535)
        throw Error("Enemy squad library must contain complete 35-byte records");
}
ScenarioResources ScenarioResources::load(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision() != 0)
        throw Error("Scenario resources currently require US revision 0");
    QFile owners(":/game/scenarios/resource-owners.json");
    if (!owners.open(QIODevice::ReadOnly))
        throw Error("Missing scenario resource reader catalog");
    for (const auto &value :
         QJsonDocument::fromJson(owners.readAll()).object()["spans"].toArray()) {
        const auto span = value.toObject();
        if (sha256(rom.bytes().mid(span["start"].toInt(), span["size"].toInt())) !=
            span["sha256"].toString().toLatin1())
            throw Error(span["name"].toString() +
                        " differs from the supported resource path; redirected archives cannot "
                        "be read as original resources");
    }
    ScenarioResources result;
    const auto enemyEntry = linkedEntry(rom, enemyLink), esetEntry = linkedEntry(rom, esetLink);
    const auto enemies = group(rom, enemyEntry);
    if (enemies.size() != 1 || enemies.first().name != "enemydat.bin")
        throw Error("Enemy resource does not contain its squad library");
    result.enemy_ = enemies.first();
    result.enemyData_ = result.enemy_.decode(rom);
    enemySize(result.enemyData_);
    result.members_ = group(rom, esetEntry);
    result.selectors_ = selectors(rom);
    const auto h = rom.bytes().mid(descriptorOffset, descriptorSize);
    if (h.startsWith("LC05SCEN")) {
        if (h.size() != descriptorSize || u32(h, 8) != 1 || h.mid(52, 8) != QByteArray(8, 0) ||
            crc32(h.first(60)) != u32(h, 60))
            throw Error("Damaged scenario resource descriptor");
        const ScenarioPlacement p{u32(h, 12), u32(h, 16)};
        if (p.offset < 0x2800000 || p.offset % 16 || !p.allocation || p.allocation % 16 ||
            quint64(p.offset) + p.allocation > 0x4000000 ||
            quint64(p.offset) + p.allocation > quint64(rom.bytes().size()))
            throw Error("Scenario resource allocation exceeds the cartridge");
        const auto data = rom.bytes().mid(p.offset, p.allocation);
        const auto eo = u32(h, 20), en = u32(h, 24), so = u32(h, 28), sn = u32(h, 32),
                   bo = u32(h, 48);
        if (crc32(data) != u32(h, 36) || crc32(result.selectors_) != u32(h, 40) || eo != 0 ||
            en < 4 || so != ((en + 15u) & ~15u) || sn < 4 || quint64(so) + sn != bo ||
            quint64(bo) + keyCount > p.allocation || enemyEntry != p.offset + eo ||
            esetEntry != p.offset + so || u32(data, eo) + 4 != en || u32(data, so) + 4 != sn)
            throw Error("Scenario resources differ from their declared allocation");
        result.baselineCount_ = int(u32(h, 44));
        result.baselineSelectors_ = data.mid(bo, keyCount);
        result.placement_ = p;
    } else {
        result.baselineCount_ = int(result.members_.size());
        result.baselineSelectors_ = result.selectors_;
    }
    if (result.baselineCount_ < 63 || result.baselineCount_ > result.members_.size())
        throw Error("Scenario collection has an invalid original member directory");
    for (int key = 0; key < keyCount; ++key)
        if (u8(result.selectors_, key) >= result.members_.size() ||
            u8(result.baselineSelectors_, key) >= result.baselineCount_)
            throw Error("Scenario selector points outside its archive collection");
    for (const auto &member : result.members_) {
        if (!member.name.startsWith("eset") || !member.name.endsWith(".bin"))
            throw Error("Scenario collection contains an unexpected resource");
        Eset::decode(member.decode(rom));
    }
    return result;
}
int ScenarioResources::selector(int key) const {
    if (key < 0 || key >= keyCount)
        throw Error("Scenario key is outside the resource table");
    return u8(selectors_, key);
}
QByteArray ScenarioResources::scenario(const Rom &rom, int key) const {
    return members_[selector(key)].decode(rom);
}
static QByteArray envelope(const QByteArray &data) {
    QByteArray result(4, 0);
    put32(result, 0, quint32(data.size()));
    return result + data;
}
QByteArray ScenarioResources::rebuild(const Rom &rom, const QByteArray &enemyData,
                                      const QMap<int, QByteArray> &edits) const {
    const auto current = load(rom);
    if (current.selectors_ != selectors_ || current.enemyData_ != enemyData_ ||
        current.members_.size() != members_.size() || current.enemy_.offset != enemy_.offset)
        throw Error("Scenario source changed since it was loaded");
    enemySize(enemyData);
    bool changed = enemyData != enemyData_;
    for (auto it = edits.begin(); it != edits.end(); ++it) {
        if (it.key() < 1 || it.key() >= keyCount || it.key() == 54)
            throw Error("Choose a supported playable scenario for editing");
        Eset::decode(it.value());
        changed |= it.value() != scenario(rom, it.key());
    }
    if (!changed)
        return rom.bytes();
    const auto h = rom.bytes().mid(descriptorOffset, descriptorSize);
    if (!placement_ && h != QByteArray(descriptorSize, char(-1)))
        throw Error("Scenario descriptor space belongs to another modification");

    // Original members retain their ordinals and exact stored bytes. Added versions
    // are compacted on every rebuild, so repeated edits do not consume more indices.
    QByteArray collection, nextSelectors = baselineSelectors_;
    QVector<QByteArray> decoded;
    for (int i = 0; i < baselineCount_; ++i) {
        const auto &m = members_[i];
        collection += rom.bytes().mid(m.offset, m.headerSize + m.storedSize);
        decoded << m.decode(rom);
    }
    QMap<QPair<int, QByteArray>, int> added;
    for (int key = 0; key < keyCount; ++key) {
        const auto desired = edits.contains(key) ? edits[key] : scenario(rom, key);
        const int original = u8(baselineSelectors_, key);
        if (desired == decoded[original])
            continue;
        const QPair<int, QByteArray> identity{original, desired};
        if (!added.contains(identity)) {
            const int ordinal = baselineCount_ + int(added.size());
            if (ordinal > 127)
                throw Error("Scenario collection exceeds the original signed member index");
            added[identity] = ordinal;
            collection += members_[original].rebuild(rom, desired);
        }
        nextSelectors[key] = char(added[identity]);
    }
    collection += char(0);
    const auto enemyStored =
        enemyData == enemyData_
            ? rom.bytes().mid(enemy_.offset, enemy_.headerSize + enemy_.storedSize)
            : enemy_.rebuild(rom, enemyData);
    auto payload = envelope(enemyStored + char(0));
    const auto enemySize = quint32(payload.size());
    payload += QByteArray((16 - payload.size() % 16) % 16, char(-1));
    const auto esetOffset = quint32(payload.size());
    payload += envelope(collection);
    const auto esetSize = quint32(payload.size()) - esetOffset;
    const auto baselineOffset = quint32(payload.size());
    payload += baselineSelectors_;
    payload += QByteArray((16 - payload.size() % 16) % 16, char(-1));
    const quint32 allocation = quint32(payload.size());
    const bool tail = placement_ && quint64(placement_->offset) + placement_->allocation ==
                                        quint64(rom.bytes().size());
    const quint32 offset = placement_ && (allocation <= placement_->allocation || tail)
                               ? placement_->offset
                               : quint32((rom.bytes().size() + 15) & ~qsizetype(15));
    if (offset < 0x2800000 || quint64(offset) + allocation > 0x4000000)
        throw Error("Rebuilt scenario resources exceed the cartridge");
    auto image = rom.bytes();
    if (placement_)
        image.replace(placement_->offset, placement_->allocation,
                      QByteArray(placement_->allocation, char(-1)));
    if (tail && offset == placement_->offset)
        image.resize(offset);
    if (quint64(image.size()) < quint64(offset) + allocation)
        image += QByteArray(offset + allocation - image.size(), char(-1));
    image.replace(offset, allocation, payload);
    put32(image, enemyLink + 4, offset - keyBase);
    put32(image, esetLink + 4, offset + esetOffset - keyBase);
    for (int key = 0; key < keyCount; ++key)
        put8(image, selectorTable + key * selectorStride + 8, u8(nextSelectors, key));
    QByteArray descriptor(descriptorSize, 0);
    descriptor.replace(0, 8, "LC05SCEN");
    put32(descriptor, 8, 1);
    put32(descriptor, 12, offset);
    put32(descriptor, 16, allocation);
    put32(descriptor, 24, enemySize);
    put32(descriptor, 28, esetOffset);
    put32(descriptor, 32, esetSize);
    put32(descriptor, 36, crc32(payload));
    put32(descriptor, 40, crc32(nextSelectors));
    put32(descriptor, 44, quint32(baselineCount_));
    put32(descriptor, 48, baselineOffset);
    put32(descriptor, 60, crc32(descriptor.first(60)));
    image.replace(descriptorOffset, descriptorSize, descriptor);
    const auto reopened = Rom::fromBytes(image);
    const auto check = load(reopened);
    if (check.enemyData_ != enemyData || check.selectors_ != nextSelectors)
        throw Error("Scenario resource rebuild readback failed");
    for (auto it = edits.begin(); it != edits.end(); ++it)
        if (check.scenario(reopened, it.key()) != it.value())
            throw Error("Scenario archive differs from the authored data");
    return image;
}
} // namespace lordly
