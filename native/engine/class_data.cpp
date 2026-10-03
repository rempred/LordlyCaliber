#include "engine/class_data.h"
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
}
QByteArray ClassPlacement::descriptor(const QByteArray &data, const QByteArray &code) const {
    validatePlacement(*this);
    if (data.size() != layout.end - layout.indexBase || code.size() != codeBytes)
        throw Error("Class allocation sizes disagree with their payloads");
    auto paddedCode = code;
    paddedCode.append(QByteArray(codeCapacity - code.size(), 0));
    QByteArray header(descriptorSize, 0);
    header.replace(0, 8, "LC05CLAS");
    put32(header, 8, 1);
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
    put32(header, 124, crc32(header.first(124)));
    return header;
}
std::optional<ClassPlacement> ClassPlacement::read(const Rom &rom) {
    const auto header = rom.bytes().mid(descriptorRomOffset, descriptorSize);
    if (!header.startsWith("LC05CLAS"))
        return std::nullopt;
    if (rom.revision() != 0 || header.size() != descriptorSize || u32(header, 8) != 1 ||
        u32(header, 124) != crc32(header.first(124)) || header.mid(84, 40) != QByteArray(40, 0))
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
    result.directory = u32(header, 72);
    result.directoryCapacity = int(u32(header, 76));
    result.codeBytes = u32(header, 80);
    validatePlacement(result);
    if (u32(header, 16) != l.end - l.indexBase ||
        quint64(result.dataRomOffset) + u32(header, 16) > quint64(rom.bytes().size()) ||
        crc32(rom.bytes().mid(result.dataRomOffset, u32(header, 16))) != u32(header, 52) ||
        crc32(rom.bytes().mid(codeRomOffset, codeCapacity)) != u32(header, 56))
        throw Error("Relocated class data or initialization code has changed outside its rebuild");
    return result;
}
QByteArray ClassPlacement::canonicalReferences(const QByteArray &bytes, qsizetype romStart) const {
    QFile file(":/game/classes/relocations.json");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing class relocation catalog");
    const auto catalog = QJsonDocument::fromJson(file.readAll()).object();
    const auto original = catalog["originalBases"].toObject();
    const QMap<QString, quint32> current = {{"classes", layout.indexBase},
                                            {"appearances", layout.appearances},
                                            {"directory", directory}};
    auto result = bytes;
    for (const auto value : catalog["owners"].toArray()) {
        const auto owner = value.toObject();
        if (owner["start"].toInt() != romStart)
            continue;
        if (owner["size"].toInt() != bytes.size())
            throw Error("Class reference owner extent has changed");
        for (const auto entry : owner["references"].toArray()) {
            const auto ref = entry.toObject();
            const auto group = ref["group"].toString();
            const auto hi = ref["high"].toInt(), lo = ref["low"].toInt();
            const auto high = u32(bytes, hi), low = u32(bytes, lo);
            const auto address = current.value(group) + quint32(ref["addend"].toInt());
            const auto retail =
                quint32(original[group].toDouble()) + quint32(ref["addend"].toInt());
            const quint32 carry = low >> 26 == 13 ? 0 : 0x8000;
            if (!current.contains(group) || high >> 26 != 15 ||
                (high & 65535) != (address + carry) >> 16 || (low & 65535) != (address & 65535))
                throw Error("Class reference does not match its relocation descriptor");
            put32(result, hi, (high & 0xffff0000U) | ((retail + carry) >> 16));
            put32(result, lo, (low & 0xffff0000U) | (retail & 65535));
        }
    }
    return result;
}
void ClassData::validate(const ClassAsset &asset) {
    if (asset.record.size() != recordSize || asset.appearance.size() != appearanceSize)
        throw Error("Class data needs a 72-byte record and a 6-byte appearance record");
    if (asset.name.isEmpty() || asset.name.contains(char(0)))
        throw Error("Class names must be nonempty encoded text without a terminator");
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
