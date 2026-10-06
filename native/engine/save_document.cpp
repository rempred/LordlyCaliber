#include "engine/save_document.h"
#include "engine/class_data.h"
#include "game/classes/save_format.h"
#include <QtEndian>
#include <QSet>
#include <QtZlib/zlib.h>
#include <algorithm>
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
                {"count", "Count", 2, 2, 0, battery() ? 127U : 65535U}};
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
        result << unit;
    }
    return result;
}
bool SaveDocument::canEditUnits() const {
    return battery() && (slot_ == 0 || slot_ == 1);
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
        QSet<int> occupied;
        QVector<QPair<int, int>> sizes;
        int capacity = 0;
        for (int i = 0; i < 5; ++i) {
            const int id = unit.members[i], cell = cells[i];
            if (!id) {
                if (cell != unit.cells[i])
                    throw Error("Preserve the unused member's formation byte");
                continue;
            }
            if (id >= 100)
                throw Error("Formation editing for special member IDs is not yet supported");
            if (cell < 0 || cell > 8 || occupied.contains(cell))
                throw Error("Each member needs a different formation cell");
            occupied.insert(cell);
            const int type = u8(ram_, roster + id * stride + 17);
            if (type < 1 || type >= classes.entries().size())
                throw Error("Squad member refers to an unavailable character or class");
            const int size = classes.unitSize(type, u8(ram_, roster + id * stride + 18));
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
    const int at = 0x1971f0 + index * 25;
    auto record = ram_.mid(at, 25);
    for (int i = 0; i < 5; ++i)
        put8(record, 7 + i, quint8(cells[i]));
    put8(record, 12, quint8(unity));
    put8(record, 23, quint8(tactic));
    return planBytes(at, record);
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
