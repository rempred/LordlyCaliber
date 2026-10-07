#include "engine/item_data.h"
#include "engine/class_data.h"
namespace lordly {
static void validateItem(const ItemAsset &a) {
    if (a.record.size() != 32 || a.name.isEmpty() || a.name.size() > 128 ||
        a.name.contains(char(0)) || a.name.contains("\r\n"))
        throw Error("Item requires a 32-byte record and a nonempty encoded name");
}
ItemData ItemData::load(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Expandable items currently require US revision zero");
    const auto p = ClassPlacement::read(rom);
    const bool placed = p && p->layout.items;
    const quint32 base = placed ? p->layout.items : retailBase;
    const quint32 at = placed ? p->dataRomOffset + base - p->layout.indexBase : 0x62300;
    const int count = placed ? p->layout.itemCount : retailCount;
    const quint32 names = placed ? base + 12 + count * 32 : 0x8018b4b0;
    const quint32 end = placed ? p->layout.end : 0x8018c40c;
    ItemData result;
    result.prefix_ = rom.bytes().mid(at, 12);
    for (int id = 0; id < count; ++id) {
        ItemAsset a;
        a.record = rom.bytes().mid(at + 12 + id * 32, 32);
        const auto pointer = u32(a.record, 0);
        if ((pointer < names || pointer >= end) && !(id == 0 && !placed && pointer == 0x8019037c))
            throw Error("Item name lies outside its allocation");
        const auto start = placed ? qsizetype(at) + pointer - base : pointer - 0x8012a100U;
        const auto stop = rom.bytes().indexOf(char(0), start);
        if (stop < start || stop - start > 128 || (pointer < end && stop - start >= end - pointer))
            throw Error("Unterminated item name");
        a.name = rom.bytes().mid(start, stop - start);
        validateItem(a);
        result.entries_ << a;
    }
    return result;
}
void ItemData::set(int id, const ItemAsset &a) {
    if (id < 0 || id >= entries_.size())
        throw Error("Item ID is outside its table");
    validateItem(a);
    entries_[id] = a;
}
int ItemData::append(int source, const QByteArray &name) {
    if (source < 1 || source >= entries_.size() || entries_.size() >= maximumCount)
        throw Error("Choose an existing item; equipment IDs 1–510 are available");
    auto a = entries_[source];
    a.name = name;
    validateItem(a);
    entries_ << a;
    return int(entries_.size()) - 1;
}
QByteArray ItemData::recordsWithPrefix() const {
    auto bytes = prefix_;
    for (const auto &a : entries_)
        bytes += a.record;
    return bytes;
}
QByteArray ItemData::build(quint32 base) const {
    auto bytes = recordsWithPrefix();
    for (int id = 0; id < entries_.size(); ++id) {
        validateItem(entries_[id]);
        put32(bytes, 12 + id * 32, base + quint32(bytes.size()));
        bytes += entries_[id].name;
        bytes += char(0);
    }
    bytes += QByteArray((16 - bytes.size() % 16) % 16, 0);
    if (base % 16 || quint64(base) + bytes.size() > 0x80400000)
        throw Error("Expanded items exceed the reserved 4 MB RAM allocation");
    return bytes;
}
} // namespace lordly
