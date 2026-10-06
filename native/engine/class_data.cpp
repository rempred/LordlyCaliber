#include "engine/class_data.h"
#include "engine/class_text.h"
#include "engine/expanded_promotion_resource.h"
#include "engine/promotion.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>

namespace lordly {
static constexpr int retailCount = 165, recordSize = 72, appearanceSize = 6;
static constexpr quint32 nameRamStart = 0x8018febc, nameRamEnd = 0x80190378;
static constexpr qsizetype nameRomStart = 0x65dbc;
static void validatePlacement(const ClassPlacement &p) {
    p.saves.validate();
    if (p.residentBytes) {
        const auto first =
            p.saves.installed() ? p.saves.endAddress() : ClassPlacement::residentBegin;
        if (p.residentAddress != first || p.residentAddress % 16 || p.residentBytes % 16 ||
            quint64(p.residentAddress) + p.residentBytes > ClassPlacement::residentEnd)
            throw Error("Resident extension overlaps saves or exceeds reclaimed class storage");
    } else if (p.residentAddress || p.residentChecksum) {
        throw Error("Empty resident extension has placement metadata");
    }
    const auto &l = p.layout;
    if (l.count < 165 || l.count > 256 || l.indexBase != 0x80243db0 ||
        l.records != l.indexBase + 72 || l.appearances % 16 || l.aliases % 16 || l.names % 16 ||
        l.discovery % 16 || l.end % 16 || l.end > 0x80400000 ||
        l.appearances < quint64(l.records) + l.count * 72 ||
        l.aliases < quint64(l.appearances) + l.count * 6 ||
        l.names < quint64(l.aliases) + l.count || l.discovery <= l.names ||
        l.end < quint64(l.discovery) + 32 || p.directory != 0x80187e00 ||
        p.directoryCapacity != 10 || p.dataRomOffset < 0x2800000 || p.dataRomOffset % 16 ||
        quint64(p.dataRomOffset) + l.end - l.indexBase > 0x4000000 || !p.codeBytes ||
        p.codeBytes > ClassPlacement::codeCapacity || p.codeBytes % 4)
        throw Error("Unsupported relocated class layout");
    for (const auto entry : {p.bootEntry, p.heapEntry, p.secondaryEntry})
        if (entry % 4 || entry < ClassPlacement::codeRamAddress ||
            entry >= ClassPlacement::codeRamAddress + p.codeBytes)
            throw Error("Class initialization entry is outside its code allocation");
    if (p.bootEntry == p.heapEntry || p.bootEntry == p.secondaryEntry ||
        p.heapEntry == p.secondaryEntry)
        throw Error("Class initialization entries overlap");
    if (p.descriptionEntry &&
        (!p.saves.installed() || p.descriptionEntry % 4 ||
         p.descriptionEntry < ClassPlacement::codeRamAddress ||
         p.descriptionEntry + 64 != ClassPlacement::codeRamAddress + p.codeBytes))
        throw Error("Class description routing is outside its resident code allocation");
}
QByteArray ClassPlacement::descriptor(const QByteArray &data, const QByteArray &code) const {
    validatePlacement(*this);
    if (data.size() != layout.end - layout.indexBase || code.size() != codeBytes)
        throw Error("Class allocation sizes disagree with their payloads");
    auto paddedCode = code;
    paddedCode.append(QByteArray(codeCapacity - code.size(), 0));
    QByteArray header(descriptorSize, 0);
    header.replace(0, 8, "LC05CLAS");
    put32(header, 8, residentBytes ? 4 : descriptionEntry ? 3 : saves.installed() ? 2 : 1);
    put32(header, 12, dataRomOffset);
    put32(header, 16, quint32(data.size()));
    put32(header, 20, layout.count);
    put32(header, 24, layout.indexBase);
    put32(header, 28, layout.records);
    put32(header, 32, layout.appearances);
    put32(header, 36, layout.aliases);
    put32(header, 40, layout.names);
    put32(header, 44, layout.discovery);
    put32(header, 48, layout.end);
    put32(header, 52, crc32(data));
    put32(header, 56, crc32(paddedCode));
    put32(header, 60, bootEntry);
    put32(header, 64, heapEntry);
    put32(header, 68, secondaryEntry);
    put32(header, 72, directory);
    put32(header, 76, directoryCapacity);
    put32(header, 80, codeBytes);
    put32(header, 84, saves.codeBytes);
    put32(header, 88, saves.codeChecksum);
    put32(header, 92, saves.ownerChecksum);
    put32(header, 96, saves.version >= 2 ? saves.version : 0);
    put32(header, 100, descriptionEntry);
    put32(header, 104, residentAddress);
    put32(header, 108, residentBytes);
    put32(header, 112, residentChecksum);
    put32(header, 124, crc32(header.first(124)));
    return header;
}
std::optional<ClassPlacement> ClassPlacement::read(const Rom &rom) {
    const auto header = rom.bytes().mid(descriptorRomOffset, descriptorSize);
    if (!header.startsWith("LC05CLAS"))
        return std::nullopt;
    const auto version = u32(header, 8);
    if (rom.revision() != 0 || header.size() != descriptorSize || version < 1 || version > 4 ||
        u32(header, 124) != crc32(header.first(124)) || header.mid(116, 8) != QByteArray(8, 0) ||
        (version < 4 && header.mid(104, 12) != QByteArray(12, 0)) ||
        (version == 4 && !u32(header, 108)) || (version < 3 && u32(header, 100)) ||
        (version == 3 && !u32(header, 100)) ||
        (version == 1 && header.mid(84, 16) != QByteArray(16, 0)) ||
        ((version == 2 || version == 3) && !u32(header, 84)) ||
        (version == 4 && !u32(header, 84) && header.mid(84, 16) != QByteArray(16, 0)))
        throw Error("Unrecognized or damaged relocated class descriptor");
    ClassPlacement result;
    auto &l = result.layout;
    result.dataRomOffset = u32(header, 12);
    l.count = int(u32(header, 20));
    l.indexBase = u32(header, 24);
    l.records = u32(header, 28);
    l.appearances = u32(header, 32);
    l.aliases = u32(header, 36);
    l.names = u32(header, 40);
    l.discovery = u32(header, 44);
    l.end = u32(header, 48);
    result.bootEntry = u32(header, 60);
    result.heapEntry = u32(header, 64);
    result.secondaryEntry = u32(header, 68);
    result.descriptionEntry = u32(header, 100);
    result.directory = u32(header, 72);
    result.directoryCapacity = int(u32(header, 76));
    result.codeBytes = u32(header, 80);
    result.saves = {u32(header, 84), u32(header, 88), u32(header, 92)};
    result.saves.version = result.saves.installed() ? (u32(header, 96) ? u32(header, 96) : 1) : 0;
    result.residentAddress = u32(header, 104);
    result.residentBytes = u32(header, 108);
    result.residentChecksum = u32(header, 112);
    validatePlacement(result);
    if (result.residentBytes &&
        crc32(rom.bytes().mid(result.residentAddress - residentDelta, result.residentBytes)) !=
            result.residentChecksum)
        throw Error("Resident extension differs from its declared allocation");
    result.saves.verify(rom.bytes());
    if (result.descriptionEntry)
        verifyClassDescriptionRouting(rom.bytes(), result.descriptionEntry);
    if (u32(header, 16) != l.end - l.indexBase ||
        quint64(result.dataRomOffset) + u32(header, 16) > quint64(rom.bytes().size()) ||
        crc32(rom.bytes().mid(result.dataRomOffset, u32(header, 16))) != u32(header, 52) ||
        crc32(rom.bytes().mid(codeRomOffset, codeCapacity)) != u32(header, 56))
        throw Error("Relocated class data or initialization code has changed outside its rebuild");
    const auto slot =
        rom.bytes().mid(ExpandedPromotionResource::romOffset, ExpandedPromotionResource::slotSize);
    if (slot.size() == ExpandedPromotionResource::slotSize &&
        u32(slot, slot.size() - 64) == 0x4c43464d && u32(slot, slot.size() - 60) == 2) {
        const auto expanded = ExpandedPromotionResource::decode(slot);
        if (expanded.data.classCount != l.count)
            throw Error("Expanded class records and menus have different counts");
        expanded.inventory.requireComplete();
        expanded.inventory.verify(rom, ClassRuntimeInventory::dataBases(l));
        result.runtimeInventory = expanded.inventory.encode();
    }
    return result;
}
QJsonArray classOwnerReferences(const QJsonObject &owner, const QByteArray &bytes,
                                const QMap<QString, quint32> &bases,
                                const QJsonObject &originalBases) {
    for (const auto &value : promotionOwners()["owners"].toArray()) {
        const auto source = value.toObject();
        if (source["start"] != owner["start"] || source["size"] != owner["size"] ||
            !source.contains("cReferences"))
            continue;
        auto normalized = bytes;
        bool matches = bytes.size() == owner["size"].toInt();
        const auto references = source["cReferences"].toArray();
        for (const auto &entry : references) {
            const auto ref = entry.toObject();
            const auto group = ref["group"].toString();
            const auto hi = ref["high"].toInt(), lo = ref["low"].toInt();
            if (!bases.contains(group) || !originalBases.contains(group) || hi < 0 || lo < 0 ||
                hi + 4 > bytes.size() || lo + 4 > bytes.size()) {
                matches = false;
                break;
            }
            const auto high = u32(bytes, hi), low = u32(bytes, lo);
            const quint32 carry = low >> 26 == 13 ? 0 : 0x8000;
            const auto address = bases[group] + quint32(ref["addend"].toInt());
            const auto retail =
                quint32(originalBases[group].toDouble()) + quint32(ref["addend"].toInt());
            if (high >> 26 != 15 || (high & 65535) != (address + carry) >> 16 ||
                (low & 65535) != (address & 65535)) {
                matches = false;
                break;
            }
            put32(normalized, hi, (high & 0xffff0000U) | ((retail + carry) >> 16));
            put32(normalized, lo, (low & 0xffff0000U) | (retail & 65535));
        }
        if (matches && sha256(normalized) == source["cSourceSha256"].toString().toLatin1())
            return references;
    }
    return owner["references"].toArray();
}
static QByteArray transformReferences(const ClassPlacement &placement, const QByteArray &bytes,
                                      qsizetype romStart, bool canonical) {
    if (!placement.runtimeInventory.isEmpty()) {
        const auto inventory = ClassRuntimeInventory::decode(placement.runtimeInventory);
        for (const auto &record : inventory.owners)
            if (record.start == romStart) {
                const auto current = ClassRuntimeInventory::dataBases(placement.layout);
                const auto original = ClassRuntimeInventory::canonicalBases();
                return inventory.relocate(record.start, bytes, canonical ? current : original,
                                          canonical ? original : current);
            }
    }
    QFile file(":/game/classes/relocations.json");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing class relocation catalog");
    const auto catalog = QJsonDocument::fromJson(file.readAll()).object();
    const auto original = catalog["originalBases"].toObject();
    const QMap<QString, quint32> current = {{"classes", placement.layout.indexBase},
                                            {"appearances", placement.layout.appearances},
                                            {"directory", placement.directory}};
    auto result = bytes;
    for (const auto value : catalog["owners"].toArray()) {
        const auto owner = value.toObject();
        if (owner["start"].toInt() != romStart)
            continue;
        if (owner["size"].toInt() != bytes.size())
            throw Error("Class reference owner extent has changed");
        QMap<QString, quint32> bases = current;
        if (!canonical)
            for (auto it = bases.begin(); it != bases.end(); ++it)
                it.value() = quint32(original[it.key()].toDouble());
        for (const auto entry : classOwnerReferences(owner, bytes, bases, original)) {
            const auto ref = entry.toObject();
            const auto group = ref["group"].toString();
            const auto hi = ref["high"].toInt(), lo = ref["low"].toInt();
            const auto high = u32(bytes, hi), low = u32(bytes, lo);
            const auto relocated = current.value(group) + quint32(ref["addend"].toInt());
            const auto retail =
                quint32(original[group].toDouble()) + quint32(ref["addend"].toInt());
            const quint32 carry = low >> 26 == 13 ? 0 : 0x8000;
            const auto address = canonical ? relocated : retail;
            const auto target = canonical ? retail : relocated;
            if (!current.contains(group) || high >> 26 != 15 ||
                (high & 65535) != (address + carry) >> 16 || (low & 65535) != (address & 65535))
                throw Error("Class reference does not match its relocation descriptor");
            put32(result, hi, (high & 0xffff0000U) | ((target + carry) >> 16));
            put32(result, lo, (low & 0xffff0000U) | (target & 65535));
        }
    }
    return result;
}
QByteArray ClassPlacement::canonicalReferences(const QByteArray &bytes, qsizetype start) const {
    return transformReferences(*this, bytes, start, true);
}
QByteArray ClassPlacement::placedReferences(const QByteArray &bytes, qsizetype start) const {
    return transformReferences(*this, bytes, start, false);
}
void ClassData::validate(const ClassAsset &asset) {
    if (asset.record.size() != recordSize || asset.appearance.size() != appearanceSize)
        throw Error("Class data needs a 72-byte record and a 6-byte appearance record");
    if (asset.name.isEmpty() || asset.name.contains(char(0)))
        throw Error("Class names must be nonempty encoded text without a terminator");
}
int ClassData::unitSize(int primary, int secondary) const {
    if (primary < 0 || secondary < 0 || primary >= entries_.size() || secondary >= entries_.size())
        throw Error("Character size refers to an unavailable class");
    // Resident ROM43D1C..43D70: retain the primary class when its declared
    // secondary matches; otherwise use the character's independent secondary.
    const int selected = u8(entries_[primary].record, 69) == secondary ? primary : secondary;
    return u8(entries_[selected].record, unitSizeOffset);
}
int ClassData::carryingCapacity(int primary, int secondary) const {
    if (primary < 0 || secondary < 0 || primary >= entries_.size() || secondary >= entries_.size())
        throw Error("Character carrying capacity refers to an unavailable class");
    // Resident ROM43100..43154 uses the same pair selection as the size getter.
    const int selected = u8(entries_[primary].record, 69) == secondary ? primary : secondary;
    return u8(entries_[selected].record, itemCapacityOffset);
}
int ClassData::movementType(int primary, int secondary) const {
    if (primary < 0 || secondary < 0 || primary >= entries_.size() || secondary >= entries_.size())
        throw Error("Character movement type refers to an unavailable class");
    // Resident ROM43AD8..43B2C: full-record offset44 is body byte32.
    const int selected = u8(entries_[primary].record, 69) == secondary ? primary : secondary;
    return u8(entries_[selected].record, movementTypeOffset);
}
int ClassData::commandType(int primary, int secondary) const {
    if (primary < 0 || secondary < 0 || primary >= entries_.size() || secondary >= entries_.size())
        throw Error("Character command type refers to an unavailable class");
    // Resident ROM43DC4..43E18: same primary/secondary selection, header byte6.
    const int selected = u8(entries_[primary].record, 69) == secondary ? primary : secondary;
    return u8(entries_[selected].record, commandTypeOffset);
}
int ClassData::unitMovementType(const QVector<QPair<int, int>> &members,
                                const QByteArray &priority) const {
    if (members.size() > 5 || priority.size() != 7)
        throw Error("Unit movement requires up to five members and the ROM priority table");
    QVector<int> types(5, 0);
    for (int i = 0; i < members.size(); ++i)
        types[i] = movementType(members[i].first, members[i].second);
    // Resident ROM40F88..40FF0 searches the table in order and falls back to7.
    for (auto type : priority)
        if (types.contains(quint8(type)))
            return quint8(type);
    return 7;
}
ClassData ClassData::loadRetail(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision() != 0)
        throw Error("Relocatable class data currently supports US Rev 0");
    ClassData result;
    // Some readers form an index base 72 bytes before class zero. These bytes
    // belong to preceding native data, not an extra class. Retain them exactly.
    result.prefix_ = rom.bytes().mid(0x5dacc, recordSize);
    if (result.prefix_.size() != recordSize)
        throw Error("Truncated class table prefix");
    for (int id = 0; id < retailCount; ++id) {
        ClassAsset entry;
        entry.record = rom.bytes().mid(0x5db14 + id * recordSize, recordSize);
        entry.appearance = rom.bytes().mid(0x64be0 + id * appearanceSize, appearanceSize);
        entry.namedAlias = u8(rom.bytes(), 0xdbe18 + id);
        const auto address = u32(entry.record, 0);
        if (address < nameRamStart || address >= nameRamEnd)
            throw Error("Class name pointer requires a different runtime layout");
        const auto begin = nameRomStart + (address - nameRamStart);
        const auto end = rom.bytes().indexOf(char(0), begin);
        if (end < begin || end >= nameRomStart + (nameRamEnd - nameRamStart))
            throw Error("Class name is not terminated within its owned pool");
        entry.name = rom.bytes().mid(begin, end - begin);
        validate(entry);
        result.entries_.push_back(std::move(entry));
    }
    return result;
}
ClassData ClassData::load(const Rom &rom) {
    rom.requireSupported();
    const auto placement = ClassPlacement::read(rom);
    if (!placement)
        return loadRetail(rom);
    const auto &l = placement->layout;
    const auto bytes = rom.bytes().mid(placement->dataRomOffset, l.end - l.indexBase);
    ClassData result;
    result.prefix_ = bytes.first(72);
    for (int id = 0; id < l.count; ++id) {
        ClassAsset entry;
        entry.record = bytes.mid(l.records - l.indexBase + id * 72, 72);
        entry.appearance = bytes.mid(l.appearances - l.indexBase + id * 6, 6);
        entry.namedAlias = u8(bytes, l.aliases - l.indexBase + id);
        const auto pointer = u32(entry.record, 0);
        if (pointer < l.names || pointer >= l.discovery)
            throw Error("Relocated class name pointer is outside the names allocation");
        const auto start = pointer - l.indexBase;
        const auto end = bytes.indexOf(char(0), start);
        if (end < start || end >= l.discovery - l.indexBase)
            throw Error("Relocated class name is not terminated within the names allocation");
        entry.name = bytes.mid(start, end - start);
        validate(entry);
        result.entries_.push_back(std::move(entry));
    }
    return result;
}
int ClassData::append(int templateId, const QByteArray &encodedName) {
    if (templateId < 0 || templateId >= entries_.size())
        throw Error("Class template is outside the current table");
    if (entries_.size() >= 256)
        throw Error("Class IDs exceed the one-byte record format");
    auto entry = entries_[templateId];
    entry.name = encodedName;
    // Zero means that a character's secondary class defaults to its own ID.
    // A new ID must retain the linked template's owner for body construction;
    // the class-copy match still selects the new class's independent records.
    if (!u8(entry.record, 69) && (templateId == 10 || templateId == 25 || templateId == 123))
        put8(entry.record, 69, quint8(templateId));
    // A new ordinary class does not inherit a unique character's alias rule.
    entry.namedAlias = 0;
    validate(entry);
    entries_.push_back(std::move(entry));
    return int(entries_.size()) - 1;
}
void ClassData::set(int id, const ClassAsset &asset) {
    if (id < 0 || id >= entries_.size())
        throw Error("Class ID is outside the current table");
    validate(asset);
    entries_[id] = asset;
}
ClassDataImage ClassData::build(quint32 ramBase, quint32 ramEnd) const {
    if (entries_.size() < retailCount || entries_.size() > 256 || prefix_.size() != recordSize)
        throw Error("Invalid class table");
    if ((ramBase & 15) || ramBase < 0x80000000U || ramEnd > 0x80400000U || ramBase >= ramEnd)
        throw Error("Class data needs an aligned allocation within 4 MB RDRAM");
    ClassDataImage result;
    auto &layout = result.layout;
    layout.indexBase = ramBase;
    layout.count = int(entries_.size());
    quint64 cursor = ramBase;
    auto allocate = [&](quint64 size, quint32 alignment) {
        cursor = (cursor + alignment - 1) & ~quint64(alignment - 1);
        if (cursor > ramEnd || size > quint64(ramEnd) - cursor)
            throw Error("Rebuilt class data exceeds its available RDRAM allocation");
        const auto address = quint32(cursor);
        cursor += size;
        return address;
    };
    allocate(recordSize, 16);
    layout.records = allocate(entries_.size() * recordSize, 4);
    layout.appearances = allocate(entries_.size() * appearanceSize, 16);
    layout.aliases = allocate(entries_.size(), 16);
    layout.names = allocate(0, 16);
    QMap<QByteArray, quint32> pointers;
    QVector<quint32> namePointers;
    for (const auto &entry : entries_) {
        validate(entry);
        if (!pointers.contains(entry.name))
            pointers[entry.name] = allocate(quint64(entry.name.size()) + 1, 4);
        namePointers.push_back(pointers[entry.name]);
    }
    // Reserve all 256 discovery bits so adding rows does not change the saved
    // extension width. Initialization/loading is owned by the runtime, not by
    // authored class data.
    layout.discovery = allocate(32, 16);
    layout.end = allocate(0, 16);
    result.bytes = QByteArray(layout.end - ramBase, char(0));
    result.bytes.replace(0, prefix_.size(), prefix_);
    for (int id = 0; id < entries_.size(); ++id) {
        const auto &entry = entries_[id];
        auto record = entry.record;
        put32(record, 0, namePointers[id]);
        result.bytes.replace(layout.records - ramBase + id * recordSize, recordSize, record);
        result.bytes.replace(layout.appearances - ramBase + id * appearanceSize, appearanceSize,
                             entry.appearance);
        put8(result.bytes, layout.aliases - ramBase + id, entry.namedAlias);
    }
    for (auto it = pointers.begin(); it != pointers.end(); ++it)
        result.bytes.replace(it.value() - ramBase, it.key().size(), it.key());
    return result;
}
} // namespace lordly
