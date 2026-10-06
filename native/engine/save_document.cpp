#include "engine/save_document.h"
#include "engine/class_data.h"
#include "engine/project.h"
#include "game/army/leader.h"
#include "game/army/soldier_reserve.h"
#include "game/classes/save_format.h"
#include <QSet>
#include <QtEndian>
#include <QtZlib/zlib.h>
#include <algorithm>
#include <array>
#include <cmath>
namespace lordly {
namespace {
constexpr int roster = 0x193bc0, stride = 56;
const QByteArray rzipMagic = QByteArray::fromHex("23525a4950760123");
QByteArray swapWords(QByteArray bytes) {
    if (bytes.size() % 4)
        throw Error("Word-swapped save length is not aligned");
    for (qsizetype i = 0; i < bytes.size(); i += 4) {
        std::swap(bytes[i], bytes[i + 3]);
        std::swap(bytes[i + 1], bytes[i + 2]);
    }
    return bytes;
}
quint32 number(const QByteArray &bytes, int at, int count) {
    quint32 result = 0;
    for (int i = 0; i < count; ++i)
        result = (result << 8) | u8(bytes, at + i);
    return result;
}
void writeNumber(QByteArray &bytes, int at, int count, quint32 value) {
    for (int i = count - 1; i >= 0; --i) {
        put8(bytes, at + i, quint8(value));
        value >>= 8;
    }
}
bool rosterPresent(const QByteArray &ram) {
    const int first = roster + stride;
    return ram.size() >= 0x200000 && u8(ram, first + 17) && u8(ram, first + 19) >= 1 &&
           u8(ram, first + 19) <= 99;
}
QString name(const QByteArray &ram, int at, bool packed) {
    QString result;
    for (int i = 0; i < (packed ? 13 : 17); ++i) {
        const auto byte = u8(ram, at + i);
        if (byte == (packed ? 255 : 0))
            break;
        const int character = packed ? byte - 16 : byte;
        if (character >= 32 && character < 127)
            result += QChar(character);
        else
            result += QString("{%1}").arg(byte, 2, 16, QChar('0'));
    }
    return result;
}
int recordOffset(int slot, bool expanded) {
    return int(lc_save_record_offset(slot, expanded));
}
int recordSize(int slot, bool expanded) {
    return int(lc_save_record_size(slot, expanded));
}
} // namespace
void SaveDocument::readCodec(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Save editing currently requires the US revision-zero ROM");
    // Boot-loader DMA proves 40E80 -> 8016AF80; this is resident data, not the
    // early-boot address delta. Descriptor and command arrays remain ROM inputs.
    constexpr quint32 delta = 0x8012a100;
    int bit = 0, groups = 0;
    for (int at = 0x5d344; u32(rom.bytes(), at); at += 16) {
        if (++groups > 32)
            throw Error("Save descriptor group limit exceeded");
        const auto pointer = u32(rom.bytes(), at), step = u32(rom.bytes(), at + 4),
                   commands = u32(rom.bytes(), at + 8), count = u32(rom.bytes(), at + 12);
        if (pointer < 0x80000000 || pointer >= 0x80400000 || !count || count > 1024 || !step ||
            step > 4096 || quint64(pointer) + quint64(step) * count > 0x80400000 ||
            commands < 0x8016af80 || commands >= 0x80190f10)
            throw Error("Save descriptor points outside its qualified resident range");
        QVector<Cell> fields;
        for (int cmd = int(commands - delta);; cmd += 3) {
            const int width = u8(rom.bytes(), cmd + 1) & 127;
            if (!width)
                break;
            const int bits = u8(rom.bytes(), cmd + 2), off = u8(rom.bytes(), cmd);
            if (fields.size() >= 256 || width > 4 || bits < 1 || bits > width * 8 ||
                off + width > int(step) || cmd + 3 > 0x66e10)
                throw Error("Invalid save descriptor field");
            fields.append({off, width, bits, 0, bool(u8(rom.bytes(), cmd + 1) & 128)});
        }
        for (quint32 n = 0; n < count; ++n)
            for (auto field : fields) {
                field.address += int(pointer & 0x1fffffff) + int(n * step);
                field.bit = bit;
                bit += field.bits;
                codec_.append(field);
            }
    }
    if (groups != 21 || bit != 49433)
        throw Error("Unrecognized packed-save layout");
    // The same boot-loaded resident region holds the increment routine's table
    // at live 80187000. Month indexes are 1..15, not the old browser's 1..12.
    // Resident movement-type aggregation ROM40F88 reads seven priority bytes.
    movementOrder_ = rom.bytes().mid(0x5cebc, 7);
    monthLengths_ = rom.bytes().mid(0x5cf00, 15);
    for (auto day : monthLengths_)
        if (quint8(day) < 1 || quint8(day) > 30)
            throw Error("Unrecognized calendar month length");
}
int SaveDocument::daysInMonth(unsigned year, unsigned month) const {
    if (month < 1 || month > 15)
        throw Error("Calendar month must be 1–15");
    const unsigned shifted = (year + 1748) & 65535;
    const bool leap = shifted % 4 == 0 && (shifted % 100 != 0 || shifted % 400 == 0);
    return quint8(monthLengths_[int(month) - 1]) + (month == 3 && leap ? 1 : 0);
}
SaveDocument SaveDocument::open(const Rom &rom, const QByteArray &bytes, int classCount) {
    if (classCount < 165 || classCount > 256)
        throw Error("Invalid save class catalog");
    SaveDocument result;
    result.original_ = bytes;
    result.classCount_ = classCount;
    result.readCodec(rom);
    if (bytes.size() == 0x8000 || bytes.size() == 0x10000) {
        result.format_ = Format::Battery;
        result.data_ = bytes;
        const auto hasMagic = [](const QByteArray &b) {
            for (int at : {0x14, 0x1864, 0x1884, 0x30b4, 0x30f4})
                if (b.mid(at, 8) == "QuestOG3" || b.mid(at, 8) == "LC05OG3!")
                    return true;
            return false;
        };
        if (!hasMagic(result.data_)) {
            result.data_ = swapWords(bytes);
            result.swapped_ = true;
        }
        if (!hasMagic(result.data_))
            throw Error("No Ogre Battle 64 battery-save signature");
        const int version =
            lc_save_image_version(reinterpret_cast<const unsigned char *>(result.data_.constData()),
                                  unsigned(result.data_.size()));
        if (version < 0)
            throw Error("Incomplete or unrecognized expanded battery-save layout");
        result.expanded_ = version == 1;
        for (const auto &slot : result.saveSlots())
            if (slot.valid) {
                result.selectSlot(slot.id);
                break;
            }
        if (result.slot_ < 0)
            throw Error("No populated battery-save slot passed validation");
    } else {
        result.blob_ = bytes;
        if (bytes.startsWith(rzipMagic)) {
            if (bytes.size() < 20)
                throw Error("Truncated RZIP header");
            result.format_ = Format::Rzip;
            const quint32 chunk = qFromLittleEndian<quint32>(bytes.constData() + 8);
            const quint64 total = qFromLittleEndian<quint64>(bytes.constData() + 12);
            if (!chunk || chunk > 16 * 1024 * 1024 || total < 0x800000 || total > 128 * 1024 * 1024)
                throw Error("Invalid RZIP size limits");
            result.chunkSize_ = int(chunk);
            result.blob_.clear();
            qsizetype at = 20;
            while (quint64(result.blob_.size()) < total) {
                if (at + 4 > bytes.size())
                    throw Error("Truncated RZIP chunk header");
                const auto size = qFromLittleEndian<quint32>(bytes.constData() + at);
                at += 4;
                if (!size || quint64(at) + size > quint64(bytes.size()))
                    throw Error("Truncated RZIP chunk");
                const auto expected =
                    quint32(std::min(quint64(chunk), total - quint64(result.blob_.size())));
                QByteArray decoded(expected, 0);
                uLongf produced = expected;
                uLong consumed = size;
                const int status =
                    uncompress2(reinterpret_cast<Bytef *>(decoded.data()), &produced,
                                reinterpret_cast<const Bytef *>(bytes.constData() + at), &consumed);
                if (status != Z_OK || produced != expected || consumed != size)
                    throw Error("Invalid RZIP chunk or declared output length");
                result.blob_ += decoded;
                at += size;
            }
            if (at != bytes.size())
                throw Error("Unexpected trailing RZIP bytes");
        } else if (bytes.size() == 0x400000 || bytes.size() == 0x800000)
            result.format_ = Format::Ram;
        else if (bytes.size() > 0x800000 && bytes.size() <= 128 * 1024 * 1024)
            result.format_ = Format::State;
        else
            throw Error("Unsupported save format or size");
        result.data_ =
            result.format_ == Format::Ram ? bytes : swapWords(result.blob_.first(0x800000));
        result.ram_ = result.data_;
        if (!rosterPresent(result.ram_))
            throw Error("No populated roster at the supported runtime address");
    }
    result.originalData_ = result.data_;
    return result;
}
void SaveDocument::setClassCount(int count) {
    if (count < 165 || count > 256)
        throw Error("Invalid save class catalog");
    classCount_ = count;
}
void SaveDocument::validateClassEdits(int count) const {
    if (count < 165 || count > 256)
        throw Error("Invalid save class catalog");
    auto original = *this;
    original.data_ = originalData_;
    const auto check = [&](const QByteArray &before, const QByteArray &after) {
        for (int id = 1; id < 100; ++id)
            for (int offset : {17, 18}) {
                const int at = roster + stride * id + offset;
                if (u8(before, at) != u8(after, at) && u8(after, at) >= count)
                    throw Error(QString("Edited character %1 references class %2, which is absent "
                                        "from this ROM project")
                                    .arg(id)
                                    .arg(u8(after, at)));
            }
    };
    if (battery()) {
        for (int slot : {0, 1, 15})
            if (validSlot(slot))
                check(original.decode(slot), decode(slot));
    } else
        check(originalData_, data_);
}
void SaveDocument::validateUnitSupplyEdits(const Project &project) const {
    if (!battery())
        return;
    auto original = *this, current = *this;
    original.data_ = originalData_;
    const auto allowed = carryableItems(project);
    const auto classes = project.classData();
    for (int slot : {0, 1}) {
        if (!validSlot(slot))
            continue;
        const auto before = original.decode(slot);
        current.selectSlot(slot);
        original.selectSlot(slot);
        QSet<int> changedOwnership;
        for (int row = 0; row < 40; ++row) {
            const int at = 0x193ac0 + row * 4, id = u16(current.ram_, at);
            if (id && before.mid(at, 4) != current.ram_.mid(at, 4))
                changedOwnership << id;
        }
        for (int id = 1; id < 256; ++id) {
            const int assigned = current.consumableAssigned(id);
            if ((changedOwnership.contains(id) || assigned > original.consumableAssigned(id)) &&
                assigned > current.consumableOwned(id))
                throw Error(QString("Item %1 in slot %2 has more assigned copies than owned")
                                .arg(id)
                                .arg(slot + 1));
        }
        for (const auto &unit : current.units()) {
            const int at = 0x1971f0 + unit.index * 25 + 13;
            if (before.mid(at, 10) == current.ram_.mid(at, 10))
                continue; // Opening/exporting does not normalize an unusual original save.
            int count = 0;
            for (int i = 0; i < 10; ++i) {
                const int id = unit.supplies[i];
                count += id != 0;
                if (id != u8(before, at + i) && id != 0 && !allowed.contains(id))
                    throw Error(QString("Edited carried item %1 in slot %2, squad %3 is no longer "
                                        "supported by this ROM project")
                                    .arg(id)
                                    .arg(slot + 1)
                                    .arg(unit.index + 1));
            }
            if (count > current.unitCarryingCapacity(unit.index, classes))
                throw Error(QString("Edited carried items in slot %1, squad %2 exceed the current "
                                    "class carrying capacity")
                                .arg(slot + 1)
                                .arg(unit.index + 1));
        }
    }
}
QByteArray SaveDocument::decode(int slot) const {
    QByteArray ram(0x400000, 0);
    const int base = recordOffset(slot, expanded_) + 0x2a;
    for (const auto &cell : codec_) {
        quint32 value = 0;
        for (int b = 0; b < cell.bits; ++b) {
            const int bit = cell.bit + b;
            value = (value << 1) | ((u8(data_, base + bit / 8) >> (7 - bit % 8)) & 1);
        }
        if (cell.sign && cell.bits < 32 && (value & (1U << (cell.bits - 1))))
            value |= ~((1U << cell.bits) - 1);
        writeNumber(ram, cell.address, cell.bytes, value);
    }
    return ram;
}
bool SaveDocument::validSlot(int slot) const {
    const int at = recordOffset(slot, expanded_), size = recordSize(slot, expanded_);
    if (at < 0 || !size || at + size > data_.size())
        return false;
    if (!lc_save_record_valid(reinterpret_cast<const unsigned char *>(data_.constData() + at), size,
                              slot, expanded_))
        return false;
    if (u8(data_, at + 12) == 255 || (slot == 15 && !u32(data_, at)))
        return false;
    return rosterPresent(decode(slot));
}
QVector<SaveSlot> SaveDocument::saveSlots() const {
    if (!battery())
        return {{-1, "Current state", true, {}}};
    QVector<SaveSlot> result;
    for (int id : {0, 1, 15}) {
        const bool valid = validSlot(id);
        QString label = id == 15 ? "Suspend" : QString("Slot %1").arg(id + 1);
        if (valid)
            label += " — " + name(decode(id), roster + stride, true);
        result.append({id, label, valid, valid ? QString() : "Empty or invalid checksum/roster"});
    }
    return result;
}
void SaveDocument::selectSlot(int slot) {
    if (!battery()) {
        if (slot != -1)
            throw Error("State has no battery slots");
        return;
    }
    if (!validSlot(slot))
        throw Error("Selected save slot is empty or invalid");
    slot_ = slot;
    ram_ = decode(slot);
}
QString SaveDocument::formatName() const {
    if (battery())
        return QString("%1 · %2 layout")
            .arg(swapped_ ? "Word-swapped SRAM" : "Native SRAM")
            .arg(expanded_ ? "expanded Classes" : "retail");
    return format_ == Format::Ram    ? "Native RDRAM"
           : format_ == Format::Rzip ? "RetroArch RZIP state"
                                     : "Raw libretro state";
}
QString SaveDocument::defaultExtension() const {
    return battery()                ? (swapped_ ? ".sra" : ".SaveRAM")
           : format_ == Format::Ram ? ".bin"
                                    : ".state";
}
int SaveDocument::address(SaveTable table, int row) const {
    if (table == SaveTable::Characters && row >= 1 && row < 100 &&
        u8(ram_, roster + row * stride + 17))
        return roster + row * stride;
    if (table == SaveTable::Equipment && row >= 0 && row < (battery() ? 278 : 128))
        return (battery() ? 0x196b00 : 0x196ccc) + row * 4;
    if (table == SaveTable::Consumables && row >= 0 && row < 40)
        return 0x193ac0 + row * 4;
    if (table == SaveTable::Game && row == 0)
        return 0;
    throw Error("Invalid save record selection");
}
QVector<int> SaveDocument::rows(SaveTable table) const {
    QVector<int> result;
    if (table == SaveTable::Characters) {
        for (int i = 1; i < 100; ++i)
            if (u8(ram_, roster + i * stride + 17))
                result.append(i);
    } else if (table == SaveTable::Game)
        result.append(0);
    else {
        const int count = table == SaveTable::Consumables ? 40 : battery() ? 278 : 128;
        for (int i = 0; i < count; ++i) {
            result.append(i);
            if (!u32(ram_, address(table, i)))
                break; // Include the next free inventory row.
        }
    }
    return result;
}
QVector<SaveField> SaveDocument::fields(SaveTable table) const {
    if (table == SaveTable::Game) {
        const unsigned month = u8(ram_, 0x196a32), year = u16(ram_, 0x196a30);
        const auto days = month >= 1 && month <= 15 ? daysInMonth(year, month) : 31;
        return {{"goth", "Goth", 0x196a6c, 4, 0, 0x7fffffff},
                {"chaosFrame", "Chaos Frame", 0x1936a9, 1, 0, 100},
                {"year", "Year", 0x196a30, 2, 0, 998},
                {"month", "Month", 0x196a32, 1, 1, 15},
                {"day", "Day", 0x196a33, 1, 1, quint32(days)}};
    }
    if (table == SaveTable::Consumables)
        return {{"id", "Consumable / treasure ID", 0, 2, 0, 44},
                {"count", "Owned", 3, 1, 0, battery() ? 127U : 255U}};
    if (table == SaveTable::Equipment)
        return {{"id", "Equipment ID", 0, 2, 0, 277},
                {"owned", "Owned", 3, 1, 0, battery() ? 127U : 255U}};
    return {{"name", "Name", 0, battery() ? 13 : 17, 0, 0},
            {"class", "Primary class", 17, 1, 1, quint32(classCount_ - 1)},
            {"secondaryClass", "Secondary class", 18, 1, 0, quint32(classCount_ - 1)},
            {"level", "Level", 19, 1, 1, 99},
            {"hpMax", "Maximum HP", 22, 2, 1, 999},
            {"hpCur", "Current HP", 24, 2, 0, 999},
            {"element", "Element", 26, 1, 0, 4},
            {"alignment", "Alignment", 27, 1, 0, 100},
            {"strength", "Strength", 28, 2, 0, 999},
            {"vitality", "Vitality", 30, 2, 0, 999},
            {"intelligence", "Intelligence", 32, 2, 0, 999},
            {"mentality", "Mentality", 34, 2, 0, 999},
            {"agility", "Agility", 36, 2, 0, 999},
            {"dexterity", "Dexterity", 38, 2, 0, 999},
            {"luck", "Luck", 40, 1, 0, 100},
            {"weapon", "Weapon", 42, 2, 0, 277},
            {"body", "Body armor", 44, 2, 0, 277},
            {"offhand", "Off-hand", 46, 2, 0, 277},
            {"head", "Head slot", 48, 2, 0, 277},
            {"experience", "Experience", 53, 1, 0, 99}};
}
QVariant SaveDocument::value(SaveTable table, int row, const QString &key) const {
    const int base = address(table, row);
    for (const auto &field : fields(table))
        if (field.key == key)
            return key == "name" ? QVariant(name(ram_, base, battery()))
                                 : QVariant(number(ram_, base + field.offset, field.bytes));
    throw Error("Unknown save field");
}
SaveChange SaveDocument::planEdit(SaveTable table, int row, const QString &key,
                                  const QVariant &value) const {
    SaveChange result;
    result.slot = slot_;
    const auto list = fields(table);
    const auto it =
        std::find_if(list.begin(), list.end(), [&](const auto &f) { return f.key == key; });
    if (it == list.end())
        throw Error("Unknown save field");
    if (value == this->value(table, row, key))
        return result;
    const auto &field = *it;
    const int base = address(table, row), at = base + field.offset;
    if (table == SaveTable::Equipment || table == SaveTable::Consumables) {
        const auto currentId = u16(ram_, base);
        if (key == "id" && currentId)
            throw Error(
                "Existing inventory IDs are preserved; edit the quantity or use the empty row");
        if (key != "id" && !currentId && value.toUInt())
            throw Error("Select an item before setting its quantity");
        if (key == "id" && value.toUInt())
            for (int existing : rows(table))
                if (existing != row && u16(ram_, address(table, existing)) == value.toUInt())
                    throw Error("This item already has an inventory entry");
    }
    QByteArray replacement(field.bytes, 0);
    if (key == "name") {
        const auto text = value.toString();
        if (text.isEmpty() || text.size() >= field.bytes)
            throw Error(QString("Name needs 1–%1 printable ASCII characters").arg(field.bytes - 1));
        for (int i = 0; i < text.size(); ++i) {
            const auto ch = text[i].unicode();
            if (ch < 32 || ch > 126)
                throw Error("Name contains an unsupported character");
            replacement[i] = char(ch + (battery() ? 16 : 0));
        }
        if (battery())
            replacement[text.size()] = char(-1);
    } else {
        bool ok = false;
        const double number = value.toDouble(&ok);
        if (!ok || !std::isfinite(number) || number != std::floor(number) ||
            number < field.minimum || number > field.maximum)
            throw Error(
                QString("%1 must be %2–%3").arg(field.label).arg(field.minimum).arg(field.maximum));
        if (table == SaveTable::Characters && ((key == "hpCur" && number > u16(ram_, base + 22)) ||
                                               (key == "hpMax" && number < u16(ram_, base + 24))))
            throw Error("Current HP cannot exceed maximum HP");
        writeNumber(replacement, 0, field.bytes, quint32(number));
    }
    if (table == SaveTable::Game && (key == "year" || key == "month" || key == "day")) {
        auto date = ram_.mid(0x196a30, 4);
        date.replace(at - 0x196a30, field.bytes, replacement);
        if (u8(date, 3) < 1 || u8(date, 3) > daysInMonth(u16(date, 0), u8(date, 2)))
            throw Error(
                "That date has no such day; choose a valid day before changing month or year");
    }
    if (table == SaveTable::Consumables && key == "count") {
        const int assigned = battery() ? consumableAssigned(u16(ram_, base)) : u8(ram_, base + 2);
        if (u8(replacement, 0) < assigned)
            throw Error(
                QString("This item has %1 copies assigned to squads; owned count cannot be lower")
                    .arg(assigned));
    }
    return planBytes(at, replacement);
}
SaveChange SaveDocument::planBytes(int at, const QByteArray &replacement) const {
    SaveChange result;
    result.slot = slot_;
    const int width = int(replacement.size());
    if (replacement == ram_.mid(at, width))
        return result;
    auto next = data_;
    int first = at, size = width;
    if (battery()) {
        auto edited = ram_;
        edited.replace(at, width, replacement);
        QVector<bool> mapped(width, false);
        const int packed = recordOffset(slot_, expanded_) + 0x2a;
        for (const auto &cell : codec_) {
            if (cell.address >= at + width || cell.address + cell.bytes <= at)
                continue;
            const quint32 v = number(edited, cell.address, cell.bytes);
            const auto max = cell.bits == 32 ? 0xffffffffU : (1U << cell.bits) - 1;
            if (!cell.sign && v > max)
                throw Error("Value exceeds its packed-save field width");
            for (int i = std::max(at, cell.address);
                 i < std::min(at + width, cell.address + cell.bytes); ++i)
                mapped[i - at] = true;
            for (int i = 0; i < cell.bits; ++i) {
                const int bit = cell.bit + i, pos = packed + bit / 8, mask = 1 << (7 - bit % 8);
                const auto old = u8(next, pos);
                put8(next, pos,
                     quint8((old & ~mask) | (((v >> (cell.bits - 1 - i)) & 1) ? mask : 0)));
            }
        }
        for (int i = 0; i < width; ++i)
            if (!mapped[i] && replacement[i] != ram_[at + i])
                throw Error("Field byte is not persisted by this save format");
        first = recordOffset(slot_, expanded_);
        size = recordSize(slot_, expanded_);
        if (!lc_save_record_seal(reinterpret_cast<unsigned char *>(next.data() + first), size,
                                 slot_, expanded_))
            throw Error("Cannot sign edited save");
    } else
        next.replace(at, width, replacement);
    return difference(next, first, size);
}
SaveChange SaveDocument::difference(const QByteArray &next, int first, int size) const {
    SaveChange result;
    result.slot = slot_;
    for (int pos = first; pos < first + size;) {
        if (next[pos] == data_[pos]) {
            ++pos;
            continue;
        }
        const int start = pos;
        while (pos < first + size && next[pos] != data_[pos])
            ++pos;
        result.spans.append({start, data_.mid(start, pos - start), next.mid(start, pos - start)});
    }
    return result;
}
QVector<SaveLegion> SaveDocument::legions() const {
    QVector<SaveLegion> result;
    for (int i = 0; i < 10; ++i) {
        const int at = 0x1969b8 + i * 11;
        if ((u8(ram_, at + 1) & 0x81) != 1)
            continue;
        SaveLegion legion;
        legion.index = i;
        legion.label = u8(ram_, at);
        for (int slot = 0; slot < 5; ++slot)
            legion.units << u8(ram_, at + 2 + slot);
        result << legion;
    }
    return result;
}
SaveLegion SaveDocument::editableLegion(int index) const {
    if (!canEditUnits())
        throw Error("Legion changes require a regular battery-save slot");
    const auto all = legions();
    const auto found = std::find_if(all.begin(), all.end(),
                                    [&](const auto &legion) { return legion.index == index; });
    if (found == all.end())
        throw Error("Choose an existing saved legion");
    const auto squads = units();
    QSet<int> members;
    for (int id : found->units)
        if (id != 255) {
            if (id < 0 || id >= 30 || members.contains(id) ||
                std::none_of(
                    squads.begin(), squads.end(),
                    [&](const auto &unit) { return unit.index == id; }))
                throw Error("The legion has an invalid or duplicate squad reference");
            for (int row = 0; row < 10; ++row) {
                const int base = 0x1969b8 + row * 11;
                if (row != index && (u8(ram_, base + 1) & 1))
                    for (int slot = 0; slot < 5; ++slot)
                        if (u8(ram_, base + 2 + slot) == id)
                            throw Error("A squad is assigned to more than one legion");
            }
            members << id;
        }
    if (found->units[0] == 255 || members.isEmpty())
        throw Error("The legion has no command squad");
    return *found;
}
SaveChange SaveDocument::planDisbandLegion(int index) const {
    editableLegion(index);
    const auto all = legions();
    QSet<int> present, ordered;
    for (const auto &legion : all)
        present << legion.index;
    QVector<int> order;
    // Normal-save descriptor stores six three-bit Legion order entries, empty7.
    // Army runtime uses ten byte entries, emptyFF; its general record table has ten rows.
    for (int slot = 0; slot < 6; ++slot) {
        const int id = u8(ram_, 0x193ab0 + slot);
        if (id == 7)
            continue;
        if (!present.contains(id) || ordered.contains(id))
            throw Error("Saved legion order is inconsistent");
        ordered << id;
        if (id != index)
            order << id;
    }
    if (ordered != present)
        throw Error("Saved legion order cannot represent every active legion");
    auto candidate = *this;
    const int flag = 0x1969b8 + index * 11 + 1;
    candidate.apply(candidate.planBytes(flag, QByteArray(1, char(u8(ram_, flag) & ~1))), true);
    // B7164 clears active membership. C1D68 retains prior saved squad flag bits;
    // command restrictions must therefore use live legion references, not bit2.
    QByteArray bytes(6, 7);
    for (int i = 0; i < order.size(); ++i)
        bytes[i] = char(order[i]);
    candidate.apply(candidate.planBytes(0x193ab0, bytes), true);
    return difference(candidate.data_, recordOffset(slot_, expanded_),
                      recordSize(slot_, expanded_));
}
SaveChange SaveDocument::planDetachLegionUnit(int index, int unit) const {
    auto legion = editableLegion(index);
    const int slot = int(legion.units.indexOf(unit));
    if (slot <= 0 || unit == 255)
        throw Error(
            "Choose a follower squad; removing the command squad requires disbanding the legion");
    // B726C shifts the rear entry in the same formation column into slots1/2.
    legion.units[slot] = slot < 3 ? legion.units[slot + 2] : 255;
    if (slot < 3)
        legion.units[slot + 2] = 255;
    return planLegionMembers(legion);
}
QVector<int> SaveDocument::newLegionCommanders(const ClassData &classes) const {
    QVector<int> result;
    if (!canEditUnits())
        return result;
    for (const auto &unit : units()) {
        bool assigned = false;
        for (int row = 0; row < 10; ++row) {
            const int at = 0x1969b8 + row * 11;
            if (u8(ram_, at + 1) & 1)
                for (int slot = 0; slot < 5; ++slot)
                    assigned |= u8(ram_, at + 2 + slot) == unit.index;
        }
        const int leader = unit.members[0];
        if (assigned || leader < 1 || leader >= 100 ||
            (u8(ram_, 0x1971f0 + unit.index * 25 + 1) & 4) ||
            (u8(ram_, roster + leader * stride + 51) & 4) ||
            !u16(ram_, roster + leader * stride + 24))
            continue;
        try {
            const auto pair = memberClasses(leader);
            if (classes.commandType(pair.first, pair.second) == 2)
                result << unit.index;
        } catch (const Error &) {
            // Unknown character/class references do not qualify as commanders.
        }
    }
    return result;
}
SaveChange SaveDocument::planCreateLegion(int unit, const ClassData &classes) const {
    if (!newLegionCommanders(classes).contains(unit))
        throw Error("Choose an independent, undeployed squad with a living command-type2 leader");
    const auto all = legions();
    if (all.size() >= 6)
        throw Error("The regular save already contains six legions");
    QSet<int> present, labels, ordered;
    for (const auto &legion : all) {
        editableLegion(legion.index);
        if (legion.label < 1 || legion.label > 6 || labels.contains(legion.label))
            throw Error("Existing legion numbers are inconsistent");
        present << legion.index;
        labels << legion.label;
    }
    QByteArray order(6, 7);
    int count = 0;
    for (int slot = 0; slot < 6; ++slot) {
        const int id = u8(ram_, 0x193ab0 + slot);
        if (id == 7)
            continue;
        if (!present.contains(id) || ordered.contains(id))
            throw Error("Saved legion order is inconsistent");
        ordered << id;
        order[count++] = char(id);
    }
    if (ordered != present)
        throw Error("Saved legion order cannot represent every active legion");
    int index = 0;
    while (index < 7 && (u8(ram_, 0x1969b8 + index * 11 + 1) & 1))
        ++index;
    if (index == 7)
        throw Error("No free legion record can be represented by the regular save order");
    int label = 1;
    while (labels.contains(label))
        ++label;
    const auto squads = units();
    const auto commander = std::find_if(squads.begin(), squads.end(),
                                        [&](const auto &row) { return row.index == unit; });
    validateFormation(*commander, classes);
    QByteArray record(11, 0);
    put8(record, 0, quint8(label));
    put8(record, 1, 1);
    put8(record, 2, quint8(unit));
    for (int i = 3; i < 7; ++i)
        put8(record, i, 255);
    put8(record, 8, 1);
    auto candidate = *this;
    candidate.apply(candidate.planBytes(0x1969b8 + index * 11, record), true);
    candidate.legionCapacity(index); // Validate any supporting groups before exposing the change.
    candidate.apply(candidate.planLegionMembers({index, label, {unit, 255, 255, 255, 255}}), true);
    const int flag = 0x1971f0 + unit * 25 + 1;
    candidate.apply(candidate.planBytes(flag, QByteArray(1, char(u8(ram_, flag) | 2))), true);
    // Keep a complete unique order. Native C2090 can overwrite an earlier entry
    // when the new active index reuses a hole; its subsequent list repair changes order.
    order[count] = char(index);
    candidate.apply(candidate.planBytes(0x193ab0, order), true);
    return difference(candidate.data_, recordOffset(slot_, expanded_),
                      recordSize(slot_, expanded_));
}
int SaveDocument::legionCapacity(int index) const {
    const auto legion = editableLegion(index);
    const auto squads = units();
    const auto &commander = *std::find_if(squads.begin(), squads.end(), [&](const auto &unit) {
        return unit.index == legion.units[0];
    });
    int capacity = 1;
    QSet<int> groups;
    for (int member : commander.members)
        if (member >= 100) {
            soldiersInGroup(member);
            int references = 0;
            for (const auto &row : squads)
                references += int(std::count(row.members.begin(), row.members.end(), member));
            if (groups.contains(member) || references != 1 ||
                !u16(ram_, 0x190f84 + (member - 100) * 2))
                throw Error("The command squad has an invalid or conflicting Soldier group");
            groups << member;
            ++capacity;
        }
    return capacity;
}
SaveChange SaveDocument::planJoinLegion(int index, int unit, const ClassData &classes) const {
    auto legion = editableLegion(index);
    const auto squads = units();
    const auto found = std::find_if(squads.begin(), squads.end(),
                                    [&](const auto &row) { return row.index == unit; });
    if (found == squads.end())
        throw Error("Choose an existing independent squad");
    for (int row = 0; row < 10; ++row) {
        const int base = 0x1969b8 + row * 11;
        if (u8(ram_, base + 1) & 1)
            for (int slot = 0; slot < 5; ++slot)
                if (u8(ram_, base + 2 + slot) == unit)
                    throw Error("Detach this squad from its current legion first");
    }
    const int count = int(
        std::count_if(legion.units.begin(), legion.units.end(), [](int id) { return id != 255; }));
    if (count >= legionCapacity(index))
        throw Error(
            "The command squad needs one Soldier group for each follower; this legion is full");
    const int leader = found->members[0];
    if (leader < 1 || leader >= 100)
        throw Error("The joining squad needs an ordinary leader");
    memberClasses(leader);
    if ((u8(ram_, 0x1971f0 + unit * 25 + 1) & 4) || (u8(ram_, roster + leader * stride + 51) & 4) ||
        !u16(ram_, roster + leader * stride + 24))
        throw Error("A deployed squad or incapacitated leader cannot join a legion");
    validateFormation(*found, classes);
    const auto commander = std::find_if(squads.begin(), squads.end(), [&](const auto &row) {
        return row.index == legion.units[0];
    });
    const auto pair = memberClasses(commander->members[0]);
    if (classes.commandType(pair.first, pair.second) != 2)
        throw Error("The legion's command squad requires a leader with command type 2");
    validateFormation(*commander, classes);
    const auto slot = std::find(legion.units.begin() + 1, legion.units.end(), 255);
    if (slot == legion.units.end())
        throw Error("The legion has no empty follower position");
    *slot = unit;
    return planLegionMembers(legion);
}
SaveChange SaveDocument::planLegionMembers(const SaveLegion &legion) const {
    const int index = legion.index;
    const int at = 0x1969b8 + index * 11;
    auto record = ram_.mid(at, 11);
    QVector<int> types;
    int count = 0;
    for (int i = 0; i < 5; ++i) {
        put8(record, 2 + i, quint8(legion.units[i]));
        const int id = legion.units[i];
        const int type = id == 255 ? 0 : u8(ram_, 0x1971f0 + id * 25 + 24);
        if (type > 7)
            throw Error("A legion squad has an unsupported movement type");
        types << type;
        count += id != 255;
    }
    int movement = 7;
    for (auto type : movementOrder_)
        if (types.contains(quint8(type))) {
            movement = quint8(type);
            break;
        }
    put8(record, 9, quint8(movement));
    // BD318 produces11-count; the original normal-save codec stores its low3bits.
    put8(record, 10, quint8((11 - count) & 7));
    return planBytes(at, record);
}
void SaveDocument::updateLegionsForUnits(const QVector<int> &changed, const ClassData &classes) {
    const auto squads = units();
    for (const auto &legion : legions()) {
        if (std::none_of(changed.begin(), changed.end(),
                         [&](int id) { return legion.units.contains(id); }))
            continue;
        const int count = int(std::count_if(legion.units.begin(), legion.units.end(),
                                            [](int id) { return id != 255; }));
        if (count > legionCapacity(legion.index))
            throw Error("This change would leave the legion without enough Soldier groups; detach "
                        "a follower squad first");
        const auto commander = std::find_if(squads.begin(), squads.end(), [&](const auto &row) {
            return row.index == legion.units[0];
        });
        const auto pair = memberClasses(commander->members[0]);
        if (classes.commandType(pair.first, pair.second) != 2)
            throw Error("The legion's command squad requires a leader with command type 2");
        apply(planLegionMembers(legion), true);
    }
}
QVector<SaveUnit> SaveDocument::units() const {
    QVector<SaveUnit> result;
    for (int i = 0; i < 30; ++i) {
        const int at = 0x1971f0 + i * 25;
        // Army materialization at z64 C1820..C1830 selects these persistent rows.
        if ((u8(ram_, at + 1) & 0x81) != 1)
            continue;
        SaveUnit unit;
        unit.index = i;
        unit.label = u8(ram_, at);
        unit.unity = u8(ram_, at + 12);
        unit.tactic = u8(ram_, at + 23);
        for (int member = 0; member < 5; ++member) {
            unit.members << u8(ram_, at + 2 + member);
            unit.cells << u8(ram_, at + 7 + member);
        }
        for (int item = 0; item < 10; ++item)
            unit.supplies << u8(ram_, at + 13 + item);
        result << unit;
    }
    return result;
}
bool SaveDocument::canEditUnits() const {
    return battery() && (slot_ == 0 || slot_ == 1);
}
QVector<int> SaveDocument::carryableItems(const Project &project) {
    QVector<int> result;
    // Original Army selector BC3B8..BC464 checks resident item getter45420:
    // only nonzero inventory IDs with record byte5 == 0 appear in this list.
    const auto &items = project.records(Table::Consumables);
    for (int id = 1; id < items.size(); ++id)
        if (u8(items[id].bytes, 5) == 0)
            result << id;
    return result;
}
int SaveDocument::maximumUnitSupply(int index, int itemSlot) const {
    if (index < 0 || index >= 30 || itemSlot < 0 || itemSlot >= 10)
        throw Error("Choose a valid saved squad item slot");
    if (!battery())
        return 255;
    const int at = 0x1971f0 + index * 25 + 13 + itemSlot;
    for (const auto &field : codec_)
        if (field.address == at && field.bytes == 1 && !field.sign)
            return (1 << field.bits) - 1;
    return 0;
}
int SaveDocument::unitCarryingCapacity(int index, const ClassData &classes) const {
    for (const auto &unit : units())
        if (unit.index == index) {
            int capacity = 0;
            for (int id : unit.members)
                if (id) {
                    const auto pair = memberClasses(id);
                    capacity += classes.carryingCapacity(pair.first, pair.second);
                }
            return std::min(10, capacity);
        }
    throw Error("Choose an existing saved squad");
}
int SaveDocument::consumableOwned(int id) const {
    int count = 0, references = 0;
    for (int row = 0; row < 40; ++row) {
        const int at = 0x193ac0 + row * 4;
        if (id && u16(ram_, at) == id) {
            count = u8(ram_, at + 3);
            ++references;
        }
    }
    if (references > 1)
        throw Error("This carried item has duplicate inventory entries");
    return count;
}
int SaveDocument::consumableAssigned(int id) const {
    if (!id)
        return 0;
    int count = 0;
    // Resident 41674..41774 includes every player row with flag1, including
    // rows temporarily excluded from the Army menu by flag80.
    for (int unit = 0; unit < 30; ++unit) {
        const int at = 0x1971f0 + unit * 25;
        if (u8(ram_, at + 1) & 1)
            for (int slot = 0; slot < 10; ++slot)
                count += u8(ram_, at + 13 + slot) == id;
    }
    return count;
}
int SaveDocument::consumableAvailable(int id) const {
    return std::max(0, consumableOwned(id) - consumableAssigned(id));
}
SaveChange SaveDocument::planUnitSupplies(int index, const QVector<int> &supplies,
                                          const Project &project) const {
    if (!canEditUnits())
        throw Error("Carried-item editing requires a regular battery-save slot");
    const auto list = units();
    const auto it = std::find_if(list.begin(), list.end(),
                                 [&](const auto &unit) { return unit.index == index; });
    if (it == list.end() || supplies.size() != 10)
        throw Error("Choose an existing saved squad and ten carried-item slots");
    const auto allowed = carryableItems(project);
    int count = 0;
    for (int i = 0; i < 10; ++i) {
        const int id = supplies[i];
        if (id != it->supplies[i] && id != 0 && !allowed.contains(id))
            throw Error("Choose an item supported by the Army carrying list");
        count += id != 0;
    }
    QSet<int> checked;
    for (int id : supplies)
        if (id && !checked.contains(id)) {
            checked << id;
            const int added = int(std::count(supplies.begin(), supplies.end(), id)) -
                              int(std::count(it->supplies.begin(), it->supplies.end(), id));
            if (added > 0 && added > consumableAvailable(id))
                throw Error(
                    QString("Item %1 does not have enough unassigned copies in inventory").arg(id));
        }
    const int at = 0x1971f0 + index * 25;
    auto record = ram_.mid(at, 25);
    if (supplies != it->supplies && count > unitCarryingCapacity(index, project.classData()))
        throw Error("These carried items exceed the squad's carrying capacity");
    for (int i = 0; i < 10; ++i)
        put8(record, 13 + i, quint8(supplies[i]));
    return planBytes(at, record);
}
QVector<int> SaveDocument::reserveMembers() const {
    if (!canEditUnits())
        return {}; // Active runtime squads may have cleared their saved assignment rows.
    QSet<int> assigned;
    for (const auto &unit : units())
        for (int id : unit.members)
            assigned.insert(id);
    QVector<int> result;
    // Army C1770..C17F0 discovers characters by nonzero primary class. Squad
    // loading marks assignments separately; reserve membership has no saved bit.
    for (int id = 1; id < 100; ++id)
        if (u8(ram_, roster + id * stride + 17) && !assigned.contains(id) &&
            !(u8(ram_, roster + id * stride + 51) & 2))
            result << id;
    return result;
}
QVector<int> SaveDocument::newUnitLeaders(const ClassData &classes) const {
    QVector<int> result;
    for (int id : reserveMembers()) {
        const int at = roster + id * stride;
        const auto pair = memberClasses(id);
        // Army selection mode3 at C063C..C06B0: command type, assignment,
        // present primary class, status bit4 clear and positive current HP.
        if (pair.first < classes.entries().size() && pair.second < classes.entries().size() &&
            classes.commandType(pair.first, pair.second) && !(u8(ram_, at + 51) & 4) &&
            u16(ram_, at + 24))
            result << id;
    }
    return result;
}
SaveChange SaveDocument::planCreateUnit(int leader, int cell, const ClassData &classes) const {
    if (!canEditUnits())
        throw Error("Squad creation requires a regular battery-save slot");
    if (cell < 0 || cell > 8 || !newUnitLeaders(classes).contains(leader))
        throw Error("Choose an available reserve leader with command ability and positive HP");
    const auto list = units();
    if (list.size() >= 30)
        throw Error("All thirty player squads are in use");
    QSet<int> labels, ranks;
    for (const auto &unit : list) {
        const int rank = u8(ram_, 0x1977d8 + unit.index);
        if (unit.label < 1 || unit.label > 30 || labels.contains(unit.label) ||
            rank >= list.size() || ranks.contains(rank))
            throw Error("Existing squad numbers or Army order are inconsistent; resolve them "
                        "before creating a squad");
        labels << unit.label;
        ranks << rank;
    }
    int index = 0, label = 1;
    while (index < 30 && (u8(ram_, 0x1971f0 + index * 25 + 1) & 1))
        ++index;
    if (index == 30)
        throw Error("No free saved player-squad record is available");
    while (labels.contains(label))
        ++label;
    SaveUnit unit;
    unit.index = index;
    unit.label = label;
    unit.members = {leader, 0, 0, 0, 0};
    unit.cells = {cell, 0, 0, 0, 0};
    validateFormation(unit, classes);
    // New-squad commit B75C8..B77F0 sets first unused number, zero Unity,
    // tactic and supplies. The persistent writer clears its newly allocated row.
    QByteArray record(25, 0);
    put8(record, 0, quint8(label));
    put8(record, 1, 1);
    put8(record, 2, quint8(leader));
    put8(record, 7, quint8(cell));
    put8(record, 24, quint8(unitMovementType(unit, classes)));
    auto candidate = *this;
    candidate.apply(candidate.planBytes(0x1971f0 + index * 25, record), true);
    const int flag = roster + leader * stride + 51;
    candidate.apply(candidate.planBytes(flag, QByteArray(1, char(u8(ram_, flag) | 2))), true);
    // Persistent order is rank indexed by saved record, not a list of record IDs.
    candidate.apply(candidate.planBytes(0x1977d8 + index, QByteArray(1, char(list.size()))), true);
    return difference(candidate.data_, recordOffset(slot_, expanded_),
                      recordSize(slot_, expanded_));
}
SaveChange SaveDocument::planDisbandUnit(int index) const {
    if (!canEditUnits())
        throw Error("Squad disbanding requires a regular battery-save slot");
    const auto list = units();
    const auto found = std::find_if(list.begin(), list.end(),
                                    [&](const auto &unit) { return unit.index == index; });
    if (found == list.end())
        throw Error("Choose an existing saved squad");
    const int at = 0x1971f0 + index * 25, leader = found->members[0];
    if (leader < 1 || leader >= 100)
        throw Error("The squad needs a valid ordinary leader before disbanding");
    if (lordly_army_fixed_leader(quint8(leader), quint8(memberClasses(leader).first)))
        throw Error("The game keeps this character's class as a fixed squad leader");
    bool legion = false;
    for (int row = 0; row < 10; ++row) {
        const int base = 0x1969b8 + row * 11;
        if (u8(ram_, base + 1) & 1)
            for (int slot = 0; slot < 5; ++slot)
                legion |= u8(ram_, base + 2 + slot) == index;
    }
    if (legion)
        throw Error(
            "This squad belongs to a legion; its legion links must be updated before disbanding");
    QSet<int> ranks;
    for (const auto &unit : list) {
        const int rank = u8(ram_, 0x1977d8 + unit.index);
        if (rank >= list.size() || ranks.contains(rank))
            throw Error(
                "Existing Army order is inconsistent; resolve it before disbanding a squad");
        ranks << rank;
    }
    int pool = reserveSoldiers();
    for (int member : found->members)
        if (member) {
            memberClasses(member);
            int references = 0;
            for (const auto &unit : list)
                references += int(std::count(unit.members.begin(), unit.members.end(), member));
            if (references != 1 ||
                (member < 100 && member != leader && (u8(ram_, roster + member * stride + 51) & 2)))
                throw Error("A member has conflicting squad or leader membership");
            if (member >= 100)
                pool += soldiersInGroup(member);
        }
    if (pool > 255)
        throw Error("Disbanding these groups would overflow the saved Soldier reserve");
    auto candidate = *this;
    // The original Army loader already clears saved flag1. Disband empties the
    // active squad and the writer then skips it, retaining the inactive payload.
    candidate.apply(candidate.planBytes(at + 1, QByteArray(1, char(u8(ram_, at + 1) & ~1))), true);
    const int flag = roster + leader * stride + 51;
    candidate.apply(candidate.planBytes(flag, QByteArray(1, char(u8(ram_, flag) & ~2))), true);
    for (int member : found->members)
        if (member >= 100) {
            candidate.apply(candidate.planBytes(0x190f84 + (member - 100) * 2, QByteArray(2, 0)),
                            true);
            candidate.apply(candidate.planBytes(0x191074 + member - 100, QByteArray(1, 0)), true);
        }
    candidate.apply(candidate.planBytes(0x190f81, QByteArray(1, char(pool))), true);
    auto order = ram_.mid(0x1977d8, 30);
    const int prior = u8(order, index);
    for (const auto &unit : list)
        if (u8(order, unit.index) > prior)
            put8(order, unit.index, quint8(u8(order, unit.index) - 1));
    put8(order, index, 31);
    candidate.apply(candidate.planBytes(0x1977d8, order), true);
    // Owned consumables already include carried copies. Clearing membership
    // releases their assignment on the next native inventory rebuild.
    return difference(candidate.data_, recordOffset(slot_, expanded_),
                      recordSize(slot_, expanded_));
}
int SaveDocument::reserveSoldiers() const {
    return u8(ram_, 0x190f81);
}
int SaveDocument::soldiersInGroup(int member) const {
    if (member < 100 || member >= 220)
        throw Error("Choose a known Soldier group");
    return lordly_soldier_count(u16(ram_, 0x190f84 + (member - 100) * 2), u16(ram_, 0x190f82));
}
SaveChange SaveDocument::planSoldierGroup(int toUnit, int cell, const ClassData &classes) const {
    if (!canEditUnits())
        throw Error("Soldier reserve changes require a regular battery-save slot");
    if (cell < 0 || cell > 8 || !reserveSoldiers())
        throw Error("Choose an empty cell with Soldiers available in reserve");
    const auto list = units();
    const auto found = std::find_if(list.begin(), list.end(),
                                    [&](const auto &unit) { return unit.index == toUnit; });
    if (found == list.end())
        throw Error("Choose an existing saved squad");
    auto to = *found;
    int empty = -1;
    for (int i = 0; i < 5; ++i) {
        if (to.members[i] && to.cells[i] == cell)
            throw Error("The destination formation cell is occupied");
        if (i > 0 && !to.members[i] && empty < 0)
            empty = i;
    }
    if (empty < 0)
        throw Error("The destination squad has no free member slot");
    std::array<uint16_t, 120> hp;
    for (int i = 0; i < 120; ++i)
        hp[i] = u16(ram_, 0x190f84 + i * 2);
    const int id = lordly_soldier_free_id(hp.data());
    if (id >= 220)
        throw Error("Every saved Soldier group slot is in use");
    for (const auto &unit : list)
        if (unit.members.contains(id))
            throw Error("The first free Soldier HP slot is still assigned to a squad; resolve that "
                        "conflict first");
    const int count = std::min(3, reserveSoldiers());
    const auto newHp = lordly_soldier_created_hp(u16(ram_, 0x190f82), uint8_t(count));
    if (!newHp)
        throw Error("The saved Soldier maximum HP cannot create a living group");
    to.members[empty] = id;
    to.cells[empty] = cell;
    auto candidate = *this;
    QByteArray health(2, 0);
    put16(health, 0, newHp);
    candidate.apply(candidate.planBytes(0x190f84 + (id - 100) * 2, health), true);
    candidate.apply(candidate.planBytes(0x191074 + id - 100, QByteArray(1, 0)), true);
    candidate.apply(candidate.planBytes(0x190f81, QByteArray(1, char(reserveSoldiers() - count))),
                    true);
    candidate.apply(candidate.planMembershipChange({to}, classes, false), true);
    return difference(candidate.data_, recordOffset(slot_, expanded_),
                      recordSize(slot_, expanded_));
}
QPair<int, int> SaveDocument::memberClasses(int id) const {
    if (id <= 0 || id >= 220)
        throw Error("Formation editing for unknown member IDs is not supported");
    // Original Army placement C2FEC..C3044 supplies pair1/1 for Soldier groups.
    // The saved HP/companion arrays contain120 entries, member IDs100..219.
    if (id >= 100)
        return {1, 1};
    const int at = roster + id * stride, primary = u8(ram_, at + 17);
    if (!primary)
        throw Error("Squad member refers to an unavailable character");
    return {primary, u8(ram_, at + 18)};
}
QPair<int, int> SaveDocument::memberRuntimeClasses(int id) const {
    if (id < 100)
        return memberClasses(id);
    if (id >= 220)
        throw Error("Unknown special member classes are not supported");
    // Army movement uses character record0 for Soldiers; capacity uses pair1/1.
    return {u8(ram_, roster + 17), u8(ram_, roster + 18)};
}
void SaveDocument::validateFormation(const SaveUnit &unit, const ClassData &classes) const {
    if (unit.members.size() != 5 || unit.cells.size() != 5 || unit.members[0] < 1 ||
        unit.members[0] >= 100)
        throw Error("A saved squad needs an ordinary character as its leader");
    QSet<int> occupied, members;
    QVector<QPair<int, int>> sizes;
    int capacity = 0;
    for (int i = 0; i < 5; ++i) {
        const int id = unit.members[i], cell = unit.cells[i];
        if (!id)
            continue;
        if (members.contains(id))
            throw Error("A member cannot occupy two formation cells");
        members.insert(id);
        const auto pair = memberClasses(id);
        if (cell < 0 || cell > 8 || occupied.contains(cell))
            throw Error("Each member needs a different formation cell");
        occupied.insert(cell);
        const int size = classes.unitSize(pair.first, pair.second);
        if (size > 2)
            throw Error("Squad member uses an unknown class size");
        capacity += size == 2 ? 2 : 1;
        sizes << qMakePair(cell, size);
    }
    if (capacity > 5)
        throw Error("Squad exceeds its five formation places");
    for (const auto &a : sizes)
        if (a.second == 2)
            for (const auto &b : sizes)
                if (a.first != b.first && std::abs(a.first / 3 - b.first / 3) <= 1 &&
                    std::abs(a.first % 3 - b.first % 3) <= 1)
                    throw Error("A large member needs adjacent formation cells clear");
}
SaveChange SaveDocument::planUnitEdit(int index, const QVector<int> &cells, int unity, int tactic,
                                      const ClassData &classes) const {
    if (!canEditUnits())
        throw Error("Squad edits require a regular battery-save slot; active runtime copies are "
                    "not yet coordinated");
    const auto list = units();
    const auto it = std::find_if(list.begin(), list.end(),
                                 [&](const auto &unit) { return unit.index == index; });
    if (it == list.end() || cells.size() != 5)
        throw Error("Choose an existing saved squad");
    const auto &unit = *it;
    if ((unity != unit.unity && (unity < 0 || unity > 100)) ||
        (tactic != unit.tactic && (tactic < 0 || tactic > 3)))
        throw Error("Unity must be 0–100 and the tactic must be one of the four game choices");
    if (cells != unit.cells) {
        for (int i = 0; i < 5; ++i)
            if (!unit.members[i] && cells[i] != unit.cells[i])
                throw Error("Preserve the unused member's formation byte");
        auto changed = unit;
        changed.cells = cells;
        validateFormation(changed, classes);
    }
    const int at = 0x1971f0 + index * 25;
    auto record = ram_.mid(at, 25);
    for (int i = 0; i < 5; ++i)
        put8(record, 7 + i, quint8(cells[i]));
    put8(record, 12, quint8(unity));
    put8(record, 23, quint8(tactic));
    return planBytes(at, record);
}
SaveChange SaveDocument::planUnitLeader(int index, int member, const ClassData &classes) const {
    if (!canEditUnits())
        throw Error("Leader changes require a regular battery-save slot");
    if (member < 1 || member > 4)
        throw Error("Choose an ordinary nonleader member of this squad");
    const auto list = units();
    const auto found = std::find_if(list.begin(), list.end(),
                                    [&](const auto &unit) { return unit.index == index; });
    if (found == list.end())
        throw Error("Choose an existing saved squad");
    auto unit = *found;
    const int prior = unit.members[0], next = unit.members[member];
    if (prior < 1 || prior >= 100 || next < 1 || next >= 100 || prior == next)
        throw Error("Both leaders must be ordinary characters present in the squad");
    const auto oldClasses = memberClasses(prior), newClasses = memberClasses(next);
    if (lordly_army_fixed_leader(quint8(prior), quint8(oldClasses.first)))
        throw Error("The game keeps this character's class as a fixed squad leader");
    for (int id : {prior, next}) {
        int references = 0;
        for (const auto &saved : list)
            references += int(std::count(saved.members.begin(), saved.members.end(), id));
        if (references != 1)
            throw Error("A selected character has conflicting squad membership");
    }
    const int oldAt = roster + prior * stride + 51, newAt = roster + next * stride + 51;
    if (u8(ram_, newAt) & 2)
        throw Error("The selected member already has a conflicting leader flag");
    const int at = 0x1971f0 + index * 25;
    bool commander = false;
    for (int row = 0; row < 10; ++row) {
        const int legion = 0x1969b8 + row * 11;
        commander |= (u8(ram_, legion + 1) & 1) && u8(ram_, legion + 2) == index;
    }
    if (commander && classes.commandType(newClasses.first, newClasses.second) != 2)
        throw Error(
            "This squad commands a legion; its replacement leader must have command type 2");
    validateFormation(unit, classes);
    QVector<QPair<int, int>> others;
    for (int i = 0; i < 5; ++i)
        if (unit.members[i] && i != member)
            others << qMakePair(unit.cells[i], unit.members[i]);
    std::sort(others.begin(), others.end());
    auto record = ram_.mid(at, 25);
    // The native writer puts the leader first, then other occupied grid cells.
    for (int i = 0; i < 5; ++i) {
        put8(record, 2 + i,
             i == 0               ? quint8(next)
             : i <= others.size() ? quint8(others[i - 1].second)
                                  : 0);
        put8(record, 7 + i,
             i == 0               ? quint8(unit.cells[member])
             : i <= others.size() ? quint8(others[i - 1].first)
                                  : 0);
    }
    put8(record, 24, quint8(unitMovementType(unit, classes)));
    auto candidate = *this;
    candidate.apply(candidate.planBytes(at, record), true);
    candidate.apply(candidate.planBytes(oldAt, QByteArray(1, char(u8(ram_, oldAt) & ~2))), true);
    candidate.apply(candidate.planBytes(newAt, QByteArray(1, char(u8(ram_, newAt) | 2))), true);
    candidate.updateLegionsForUnits({index}, classes);
    return difference(candidate.data_, recordOffset(slot_, expanded_),
                      recordSize(slot_, expanded_));
}
void SaveDocument::validateTransferMember(int id, const QVector<SaveUnit> &list) const {
    if (id < 1 || id >= 220)
        throw Error("Choose an ordinary character or known Soldier group");
    memberClasses(id);
    int references = 0;
    for (const auto &unit : list)
        references += int(std::count(unit.members.begin(), unit.members.end(), id));
    if (references != 1 || (id < 100 && (u8(ram_, roster + id * stride + 51) & 2)))
        throw Error("Selected member has conflicting squad or leader membership");
    if (id >= 100 && !u16(ram_, 0x190f84 + (id - 100) * 2))
        throw Error("The selected Soldier group has no saved HP; resolve its assignment first");
}
SaveChange SaveDocument::planUnitSwap(int fromUnit, int fromMember, int toUnit, int toMember,
                                      const ClassData &classes) const {
    if (!canEditUnits())
        throw Error("Member swaps require a regular battery-save slot");
    if (fromUnit == toUnit || fromMember < 1 || fromMember > 4 || toMember < 1 || toMember > 4)
        throw Error("Choose nonleader members from two different squads");
    const auto list = units();
    const auto find = [&](int index) {
        const auto it =
            std::find_if(list.begin(), list.end(), [&](const auto &u) { return u.index == index; });
        if (it == list.end())
            throw Error("Choose an existing saved squad");
        return *it;
    };
    auto from = find(fromUnit), to = find(toUnit);
    const int firstId = from.members[fromMember], secondId = to.members[toMember];
    for (int id : {firstId, secondId})
        validateTransferMember(id, list);
    std::swap(from.members[fromMember], to.members[toMember]);
    // Direct Army exchange B7978..B7B5C and commit D6FA8..D7030 retain Unity.
    return planMembershipChange({from, to}, classes, false);
}
SaveChange SaveDocument::planUnitTransfer(int fromUnit, int fromMember, int toUnit, int cell,
                                          const ClassData &classes) const {
    if (!canEditUnits())
        throw Error("Member moves require a regular battery-save slot");
    if (fromUnit == toUnit || fromMember < 1 || fromMember > 4 || cell < 0 || cell > 8)
        throw Error("Choose a nonleader member and an empty cell in another squad");
    const auto list = units();
    auto find = [&](int index) {
        const auto it =
            std::find_if(list.begin(), list.end(), [&](const auto &u) { return u.index == index; });
        if (it == list.end())
            throw Error("Choose an existing saved squad");
        return *it;
    };
    auto from = find(fromUnit), to = find(toUnit);
    const int id = from.members[fromMember];
    validateTransferMember(id, list);
    int empty = -1;
    for (int i = 0; i < 5; ++i) {
        if (to.members[i] && to.cells[i] == cell)
            throw Error("The destination formation cell is occupied");
        if (i > 0 && !to.members[i] && empty < 0)
            empty = i;
    }
    if (empty < 0)
        throw Error("The destination squad has no free member slot");
    from.members[fromMember] = 0;
    from.cells[fromMember] = 0;
    to.members[empty] = id;
    to.cells[empty] = cell;
    // Direct Army exchange B7978..B7B5C and commit D6FA8..D7030 retain Unity.
    return planMembershipChange({from, to}, classes, false);
}
SaveChange SaveDocument::planToReserve(int fromUnit, int fromMember,
                                       const ClassData &classes) const {
    if (!canEditUnits())
        throw Error("Reserve moves require a regular battery-save slot");
    if (fromMember < 1 || fromMember > 4)
        throw Error("Choose a nonleader member to return to reserve");
    const auto list = units();
    const auto found = std::find_if(list.begin(), list.end(),
                                    [&](const auto &unit) { return unit.index == fromUnit; });
    if (found == list.end())
        throw Error("Choose an existing saved squad");
    auto from = *found;
    const int id = from.members[fromMember];
    if (id < 1 || id >= 220)
        throw Error("Choose an ordinary character or known Soldier group");
    memberClasses(id); // A dangling squad reference must not become a missing reserve character.
    int references = 0;
    for (const auto &unit : list)
        references += int(std::count(unit.members.begin(), unit.members.end(), id));
    if (references != 1 || (id < 100 && (u8(ram_, roster + id * stride + 51) & 2)))
        throw Error("Selected character has conflicting squad or leader membership");
    from.members[fromMember] = 0;
    from.cells[fromMember] = 0;
    if (id < 100)
        return planMembershipChange({from}, classes);
    const int count = soldiersInGroup(id), pool = reserveSoldiers();
    if (pool + count > 255)
        throw Error("Returning this group would overflow the saved Soldier reserve");
    auto candidate = *this;
    candidate.apply(candidate.planMembershipChange({from}, classes, false), true);
    candidate.apply(candidate.planBytes(0x190f84 + (id - 100) * 2, QByteArray(2, 0)), true);
    candidate.apply(candidate.planBytes(0x191074 + id - 100, QByteArray(1, 0)), true);
    candidate.apply(candidate.planBytes(0x190f81, QByteArray(1, char(pool + count))), true);
    return difference(candidate.data_, recordOffset(slot_, expanded_),
                      recordSize(slot_, expanded_));
}
SaveChange SaveDocument::planFromReserve(int member, int toUnit, int cell,
                                         const ClassData &classes) const {
    if (!canEditUnits())
        throw Error("Reserve moves require a regular battery-save slot");
    if (!reserveMembers().contains(member) || cell < 0 || cell > 8)
        throw Error("Choose an ordinary reserve character and an empty formation cell");
    const auto list = units();
    const auto found = std::find_if(list.begin(), list.end(),
                                    [&](const auto &unit) { return unit.index == toUnit; });
    if (found == list.end())
        throw Error("Choose an existing saved squad");
    auto to = *found;
    int empty = -1;
    for (int i = 0; i < 5; ++i) {
        if (to.members[i] && to.cells[i] == cell)
            throw Error("The destination formation cell is occupied");
        if (i > 0 && !to.members[i] && empty < 0)
            empty = i;
    }
    if (empty < 0)
        throw Error("The destination squad has no free member slot");
    to.members[empty] = member;
    to.cells[empty] = cell;
    return planMembershipChange({to}, classes);
}
int SaveDocument::unitMovementType(const SaveUnit &unit, const ClassData &classes) const {
    // Army ROMBD154..BD268 obtains five class-derived types; Soldiers use
    // character record0 here, distinct from their formation size pair1/1.
    QVector<QPair<int, int>> members;
    for (int id : unit.members)
        if (id)
            members << memberRuntimeClasses(id);
    return classes.unitMovementType(members, movementOrder_);
}
SaveChange SaveDocument::planMembershipChange(const QVector<SaveUnit> &changed,
                                              const ClassData &classes, bool resetUnity) const {
    auto candidate = *this;
    for (const auto &unit : changed) {
        validateFormation(unit, classes);
        int capacity = 0, carried = 0;
        for (int id : unit.members)
            if (id) {
                const auto pair = memberClasses(id);
                capacity += classes.carryingCapacity(pair.first, pair.second);
            }
        const int at = 0x1971f0 + unit.index * 25;
        auto record = ram_.mid(at, 25);
        for (int i = 13; i < 23; ++i)
            carried += u8(record, i) != 0;
        if (carried > capacity)
            throw Error("The changed squad would not have room for its carried items");
        for (int i = 0; i < 5; ++i) {
            put8(record, 2 + i, quint8(unit.members[i]));
            put8(record, 7 + i, quint8(unit.cells[i]));
        }
        // Ordinary reserve removal B7FC8 and placement B8294..B82A8 clear Unity.
        // The membership finalizer then recalculates active+29 -> saved+24.
        if (resetUnity)
            put8(record, 12, 0);
        put8(record, 24, quint8(unitMovementType(unit, classes)));
        candidate.apply(candidate.planBytes(at, record), true);
    }
    QVector<int> affected;
    for (const auto &unit : changed)
        affected << unit.index;
    candidate.updateLegionsForUnits(affected, classes);
    return difference(candidate.data_, recordOffset(slot_, expanded_),
                      recordSize(slot_, expanded_));
}
void SaveDocument::apply(const SaveChange &change, bool forward) {
    for (const auto &span : change.spans) {
        const auto &expected = forward ? span.before : span.after;
        if (span.offset < 0 || span.before.size() != span.after.size() ||
            data_.mid(span.offset, expected.size()) != expected)
            throw Error("Save edit no longer matches its original bytes");
    }
    auto candidate = *this;
    for (const auto &span : change.spans)
        candidate.data_.replace(span.offset, span.before.size(),
                                forward ? span.after : span.before);
    if (battery())
        candidate.selectSlot(change.slot);
    else
        candidate.ram_ = candidate.data_;
    *this = std::move(candidate);
}
QByteArray SaveDocument::serialize() const {
    if (data_ == originalData_)
        return original_;
    if (battery())
        return swapped_ ? swapWords(data_) : data_;
    if (format_ == Format::Ram)
        return data_;
    auto blob = blob_;
    blob.replace(0, data_.size(), swapWords(data_));
    if (format_ == Format::State)
        return blob;
    QByteArray out = rzipMagic;
    out.resize(20);
    qToLittleEndian(quint32(chunkSize_), out.data() + 8);
    qToLittleEndian(quint64(blob.size()), out.data() + 12);
    for (qsizetype at = 0; at < blob.size(); at += chunkSize_) {
        const auto compressed = qCompress(blob.mid(at, chunkSize_), 6).mid(4);
        QByteArray size(4, 0);
        qToLittleEndian(quint32(compressed.size()), size.data());
        out += size;
        out += compressed;
    }
    return out;
}
} // namespace lordly
