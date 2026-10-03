#include "engine/class_data.h"
#include <QMap>

namespace lordly {
static constexpr int retailCount = 165, recordSize = 72, appearanceSize = 6;
static constexpr quint32 nameRamStart = 0x8018febc, nameRamEnd = 0x80190378;
static constexpr qsizetype nameRomStart = 0x65dbc;
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
