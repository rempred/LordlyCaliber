#include "engine/direct_descriptors.h"
#include "engine/animation_routes.h"
#include <QJsonArray>
#include <algorithm>
namespace lordly {
namespace {
quint32 keyWords(quint32 high, quint32 low) {
    const auto reg = (high >> 16) & 31;
    if ((high & 0xffe00000U) != 0x3c000000U || (reg != 4 && reg != 5) || low >> 26 != 13 ||
        ((low >> 21) & 31) != reg || ((low >> 16) & 31) != reg)
        throw Error("Unrecognized class-animation resource operand");
    const auto key = ((high & 65535) << 16) | (low & 65535);
    if (!key || key % 2 || key >= 0x4000000 - AnimationRoutes::resourceBase)
        throw Error("Class-animation resource key is outside the cartridge");
    return key;
}
QByteArray rawResource(const Rom &rom, quint32 key) {
    const auto at = key + AnimationRoutes::resourceBase, size = u32(rom.bytes(), at);
    if (size < 36 || size > 65535 || quint64(at) + 4 + size > quint64(rom.bytes().size()))
        throw Error("Class-animation compound resource exceeds its envelope");
    return rom.bytes().mid(at + 4, size);
}
void selectorsValid(const QByteArray &bytes) {
    if (bytes.isEmpty())
        return;
    if (bytes.size() > 1025 || bytes.size() % 4 != 1 || u8(bytes, bytes.size() - 1) != 255)
        throw Error("Invalid class-animation selector group");
    for (int at = 0; at + 1 < bytes.size(); at += 4)
        if (u8(bytes, at) == 255)
            throw Error("Class-animation group has an early terminator");
}
int sectionEnd(const QByteArray &bytes, int begin) {
    int end = int(bytes.size());
    for (int i = 0; i < 18; ++i) {
        const int candidate = u16(bytes, i * 2);
        if (candidate > begin)
            end = std::min(end, candidate);
    }
    return end;
}
QByteArray jsonBytes(const QJsonValue &value, int count = -1) {
    if (!value.isString() || value.toString().size() > 2050)
        throw Error("Invalid saved animation bytes");
    const auto text = value.toString().toLatin1(), bytes = QByteArray::fromHex(text);
    if (bytes.toHex() != text.toLower() || (count >= 0 && bytes.size() != count))
        throw Error("Invalid saved animation bytes");
    return bytes;
}
} // namespace
const std::array<QPair<int, int>, 2> &AnimationRoutes::copySites() {
    // These consumers use header11 as a prefix size in retail. Header11 also
    // locates a live section, so expand their size operand, not that section.
    static const std::array<QPair<int, int>, 2> value{{{0x6e844, 0x6e850}, {0x1b9064, 0x1b9070}}};
    return value;
}
const std::array<QPair<int, int>, 8> &AnimationRoutes::sites() {
    static const std::array<QPair<int, int>, 8> value{{{0x6e814, 0x6e818},
                                                       {0x6e838, 0x6e840},
                                                       {0x1b9030, 0x1b9034},
                                                       {0x1b9058, 0x1b9060},
                                                       {0x1ea0bc, 0x1ea0c0},
                                                       {0x1ea0e0, 0x1ea0e8},
                                                       {0x201bb0, 0x201bb4},
                                                       {0x201bdc, 0x201be4}}};
    return value;
}
quint32 AnimationRoutes::resourceKey(const Rom &rom) {
    quint32 key = 0;
    for (const auto &[hi, lo] : sites()) {
        const auto current = keyWords(u32(rom.bytes(), hi), u32(rom.bytes(), lo));
        if (key && key != current)
            throw Error("Class-animation resource users disagree");
        key = current;
    }
    return key;
}
QByteArray AnimationRoutes::resourceBytes(const Rom &rom) {
    return load(rom).source_;
}
QByteArray AnimationRoutes::canonicalOwner(QByteArray bytes, qsizetype start) {
    for (const auto &[allocation, copy] : copySites()) {
        for (const auto &[at, opcode] :
             std::array<QPair<int, quint32>, 2>{{{allocation, 0x96040000U}, {copy, 0x96060000U}}}) {
            if (at < start || at + 4 > start + bytes.size())
                continue;
            const auto word = u32(bytes, at - start);
            if (word != (opcode | 0x16) && word != (opcode | 0x1e))
                throw Error("Unrecognized animation prefix-copy operand");
            put32(bytes, at - start, opcode | 0x16);
        }
    }
    for (const auto &[hi, lo] : sites()) {
        if (hi < start || lo < start || hi + 4 > start + bytes.size() ||
            lo + 4 > start + bytes.size())
            continue;
        const auto high = u32(bytes, hi - start), low = u32(bytes, lo - start);
        keyWords(high, low);
        put32(bytes, hi - start, (high & 0xffff0000U) | (retailKey >> 16));
        put32(bytes, lo - start, (low & 0xffff0000U) | (retailKey & 65535));
    }
    return bytes;
}
void AnimationRoutes::validateRelocations(const Rom &rom) {
    if (!rom.revision() &&
        (rom.bytes().mid(descriptorOffset, 8) == "LC05ARTE" || resourceKey(rom) != retailKey))
        load(rom);
}
QVector<AnimationRoutes::Entry> AnimationRoutes::parseEntries(const QByteArray &bytes) {
    const int begin = u16(bytes, 0), speed = u16(bytes, 2), kind = u16(bytes, 6),
              groups = u16(bytes, 14), extra = u16(bytes, 16), copy = u16(bytes, 30);
    const int count = (speed - begin) / 8;
    if (begin < 36 || begin % 2 || speed < begin || (speed - begin) % 8 || count < 165 ||
        count > 256 || copy > bytes.size() || copy < 36)
        throw Error("Class-animation handle table has an invalid extent");
    for (const auto &[offset, length] : std::array<QPair<int, int>, 5>{{{begin, count * 8},
                                                                        {speed, count * 2},
                                                                        {kind, count},
                                                                        {groups, count * 2},
                                                                        {extra, count * 2}}})
        if (offset < 36 || offset + length > copy)
            throw Error("Class-animation section lies outside the resident copy");
    const auto groupEnd = std::min(sectionEnd(bytes, groups), copy);
    QVector<Entry> result;
    for (int id = 0; id < count; ++id) {
        Entry entry;
        for (int flag = 0; flag < 4; ++flag)
            entry.handles[flag] = u16(bytes, begin + id * 8 + flag * 2);
        entry.speeds = {u8(bytes, speed + id * 2), u8(bytes, speed + id * 2 + 1)};
        entry.kind = u8(bytes, kind + id);
        entry.attributes = {u8(bytes, extra + id * 2), u8(bytes, extra + id * 2 + 1)};
        const auto relative = u16(bytes, groups + id * 2);
        if (relative != 65535) {
            int at = groups + relative;
            if (relative < count * 2 || at >= groupEnd)
                throw Error("Class-animation selector pointer exceeds its section");
            const int start = at;
            while (at < groupEnd && u8(bytes, at) != 255) {
                if (at + 4 >= groupEnd || at - start >= 1024)
                    throw Error("Class-animation selector group is unterminated");
                at += 4;
            }
            if (at >= groupEnd)
                throw Error("Class-animation selector group is unterminated");
            entry.selectors = bytes.mid(start, at - start + 1);
            selectorsValid(entry.selectors);
        }
        result << entry;
    }
    return result;
}
AnimationRoutes AnimationRoutes::load(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Class-animation routes currently require US revision zero");
    AnimationRoutes result;
    const auto key = resourceKey(rom);
    result.source_ = rawResource(rom, key);
    result.prefix_ = result.source_;
    result.rows_ = parseEntries(result.source_);
    result.root_ = DirectDescriptors::directory(rom);
    if (result.root_.size() % 4 || result.root_.size() > 4095 * 4)
        throw Error("Invalid class-animation descriptor directory");
    for (const auto &[hi, lo] : sites()) {
        result.ownerWords_[hi] = u32(rom.bytes(), hi);
        result.ownerWords_[lo] = u32(rom.bytes(), lo);
    }
    result.descriptor_ = rom.bytes().mid(descriptorOffset, descriptorSize);
    const bool owned = result.descriptor_.startsWith("LC05ARTE");
    for (const auto &[allocation, copy] : copySites()) {
        for (const auto &[at, opcode] :
             std::array<QPair<int, quint32>, 2>{{{allocation, 0x96040000U}, {copy, 0x96060000U}}}) {
            const auto word = u32(rom.bytes(), at);
            if (word != (opcode | (owned ? 0x1eU : 0x16U)))
                throw Error("Class-animation copy size does not match its resource owner");
            result.ownerWords_[at] = word;
        }
    }
    if (result.descriptor_.startsWith("LC05ARTE")) {
        const auto &d = result.descriptor_;
        if (d.size() != descriptorSize || (u32(d, 8) != 1 && u32(d, 8) != 2))
            throw Error("Invalid native class-animation allocation header");
        result.allocationOffset_ = u32(d, 12);
        result.allocationSize_ = u32(d, 16);
        const auto prefix = u32(d, 20), count = u32(d, 24);
        if (result.allocationOffset_ < 0x2800000 || result.allocationOffset_ % 16 ||
            !result.allocationSize_ || result.allocationSize_ % 16 ||
            result.allocationSize_ > 65552 ||
            quint64(result.allocationOffset_) + result.allocationSize_ >
                quint64(rom.bytes().size()) ||
            key != result.allocationOffset_ - resourceBase || prefix > u16(result.source_, 0) ||
            prefix < 36 || prefix >= quint32(result.source_.size()) ||
            count != quint32(result.rows_.size()) ||
            result.source_.size() + 4 > result.allocationSize_)
            throw Error("Native class-animation allocation has invalid bounds or references");
        const auto payload = rom.bytes().mid(result.allocationOffset_, result.allocationSize_);
        if (QByteArray::fromHex(sha256(payload)) != d.mid(64, 32) ||
            payload.mid(result.source_.size() + 4) !=
                QByteArray(payload.size() - result.source_.size() - 4, char(-1)))
            throw Error("Native class-animation allocation differs from its stored identity");
        const int mapSize = u16(result.source_, 0) - int(prefix);
        if (u32(d, 8) == 1 && mapSize)
            throw Error("Unexpected private pose mapping in version one animation data");
        result.setPrivatePoses(result.source_.mid(prefix, mapSize));
        result.loadedPrivatePoses_ = result.privatePoses_;
        result.prefix_ = result.source_.left(prefix);
        result.prefix_.replace(0, 36, d.mid(28, 36));
        parseEntries(result.prefix_);
        if (pack(result.prefix_, result.rows_, result.privatePoses_) != result.source_)
            throw Error("Native class-animation sections are not in their declared allocation");
    }
    return result;
}
AnimationRoutes::Entry AnimationRoutes::entry(int id) const {
    if (id < 0 || id > 255)
        throw Error("Class animation ID exceeds the one-byte class domain");
    if (pending_.contains(id))
        throw Error(QString("Class %1 needs an explicit animation source assignment").arg(id));
    if (edits_.contains(id))
        return edits_.value(id);
    if (id >= rows_.size())
        throw Error("This class has no animation source assignment");
    return rows_[id];
}
void AnimationRoutes::setEntry(int id, const Entry &entry) {
    if (id < 0 || id > 255)
        throw Error("Class animation ID exceeds the one-byte class domain");
    for (auto handle : entry.handles) {
        const int slot = (handle & 0xfff) - 1;
        if (slot < 0 || slot >= root_.size() / 4 || !u32(root_, slot * 4))
            throw Error("A class-animation handle selects a missing descriptor");
    }
    selectorsValid(entry.selectors);
    pending_.remove(id);
    if (id < rows_.size() && entry == rows_[id])
        edits_.remove(id);
    else
        edits_[id] = entry;
}
void AnimationRoutes::setRow(int id, const Row &row) {
    auto value = entry(id);
    value.handles = row;
    setEntry(id, value);
}
AnimationRoutes AnimationRoutes::withDirectory(const QByteArray &directory)const {
    if(directory.isEmpty()||directory.size()%4||directory.size()>4095*4)throw Error("Invalid replacement animation descriptor directory");
    auto result=*this;result.root_=directory;return result;
}
void AnimationRoutes::requireAssignment(int id) {
    if (id < 0 || id > 255)
        throw Error("Class animation ID exceeds the one-byte class domain");
    edits_.remove(id);
    pending_.insert(id);
}
void AnimationRoutes::restore(int id) {
    if (id < 0 || id > 255)
        throw Error("Class animation ID exceeds the one-byte class domain");
    edits_.remove(id);
    pending_.remove(id);
}
int AnimationRoutes::rowCount() const {
    return edits_.isEmpty() ? int(rows_.size()) : std::max(int(rows_.size()), edits_.lastKey() + 1);
}
QJsonObject AnimationRoutes::json() const {
    QJsonObject rows;
    for (auto it = edits_.cbegin(); it != edits_.cend(); ++it) {
        QJsonArray handles;
        for (auto handle : it->handles)
            handles << handle;
        QByteArray speed, extra;
        for (int i = 0; i < 2; ++i) {
            speed += char(it->speeds[i]);
            extra += char(it->attributes[i]);
        }
        rows[QString::number(it.key())] =
            QJsonObject{{"handles", handles},
                        {"speeds", QString::fromLatin1(speed.toHex())},
                        {"kind", it->kind},
                        {"attributes", QString::fromLatin1(extra.toHex())},
                        {"selectors", QString::fromLatin1(it->selectors.toHex())}};
    }
    QList<int> ids = pending_.values();
    std::sort(ids.begin(), ids.end());
    QJsonArray pending;
    for (int id : ids)
        pending << id;
    return {{"version", 1},
            {"sourceSha256", QString::fromLatin1(sha256(source_))},
            {"rows", rows},
            {"pending", pending}};
}
void AnimationRoutes::apply(const QJsonObject &object) {
    if (object["version"].toInt() != 1 ||
        object["sourceSha256"].toString().toLatin1() != sha256(source_) ||
        !object["rows"].isObject())
        throw Error("Saved class-animation routes do not match their current ROM");
    auto next = *this;
    next.edits_.clear();
    next.pending_.clear();
    const auto rows = object["rows"].toObject();
    for (auto it = rows.begin(); it != rows.end(); ++it) {
        bool ok = false;
        const auto id = it.key().toInt(&ok);
        const auto value = it.value().toObject();
        const auto handles = value["handles"].toArray();
        if (!ok || QString::number(id) != it.key() || !it.value().isObject() || handles.size() != 4)
            throw Error("Invalid saved class-animation route");
        Entry entry;
        for (int flag = 0; flag < 4; ++flag) {
            const auto number = handles[flag].toInteger(-1);
            if (number < 0 || number > 65535 || handles[flag].toDouble() != double(number))
                throw Error("Invalid animation handle");
            entry.handles[flag] = quint16(number);
        }
        const auto speeds = jsonBytes(value["speeds"], 2),
                   extra = jsonBytes(value["attributes"], 2);
        const int kind = value["kind"].toInt(-1);
        if (kind < 0 || kind > 255)
            throw Error("Invalid native class-animation byte");
        entry.speeds = {u8(speeds, 0), u8(speeds, 1)};
        entry.attributes = {u8(extra, 0), u8(extra, 1)};
        entry.kind = quint8(kind);
        entry.selectors = jsonBytes(value["selectors"]);
        next.setEntry(id, entry);
    }
    if (object.contains("pending")) {
        if (!object["pending"].isArray())
            throw Error("Invalid pending animation assignments");
        for (const auto &value : object["pending"].toArray()) {
            const int id = value.toInt(-1);
            if (id < 0 || id > 255 || next.pending_.contains(id) || next.edits_.contains(id))
                throw Error("Invalid pending animation class");
            next.requireAssignment(id);
        }
    }
    *this = std::move(next);
}
QByteArray AnimationRoutes::pack(const QByteArray &prefix, const QVector<Entry> &rows, const QByteArray &poses) {
    auto bytes = prefix + poses;
    const int count = int(rows.size());
    auto append = [&](int header, int size) {
        if (bytes.size() % 2)
            bytes += char(0);
        if (bytes.size() + size > 65535)
            throw Error("Class-animation resource exceeds its native relative offsets");
        const int at = int(bytes.size());
        put16(bytes, header * 2, quint16(at));
        bytes += QByteArray(size, 0);
        return at;
    };
    const int handles = append(0, count * 8), speed = append(1, count * 2), kind = append(3, count),
              extra = append(8, count * 2), groups = append(7, count * 2);
    for (int id = 0; id < count; ++id) {
        const auto &entry = rows[id];
        for (int flag = 0; flag < 4; ++flag)
            put16(bytes, handles + id * 8 + flag * 2, entry.handles[flag]);
        for (int direction = 0; direction < 2; ++direction) {
            put8(bytes, speed + id * 2 + direction, entry.speeds[direction]);
            put8(bytes, extra + id * 2 + direction, entry.attributes[direction]);
        }
        put8(bytes, kind + id, entry.kind);
        selectorsValid(entry.selectors);
        if (entry.selectors.isEmpty())
            put16(bytes, groups + id * 2, 65535);
        else {
            if (bytes.size() + entry.selectors.size() > 65535)
                throw Error("Class-animation selectors exceed their native offsets");
            put16(bytes, groups + id * 2, quint16(bytes.size() - groups));
            bytes += entry.selectors;
        }
    }
    if (bytes.size() % 2)
        bytes += char(0);
    if (bytes.size() > 65535)
        throw Error("Class-animation allocation exceeds its resident size field");
    put16(bytes, 30, quint16(bytes.size()));
    return bytes;
}
void AnimationRoutes::setPrivatePoses(const QByteArray &bytes) {
    if (bytes.size() % 8 || bytes.size() > 256 * 8)
        throw Error("Invalid private pose mapping size");
    quint32 previous = 0;
    for (int at = 0; at < bytes.size(); at += 8) {
        const auto key = u32(bytes, at);
        const int secondary = qint16(u16(bytes, at + 4));
        if ((at && key <= previous) || !u8(bytes, at) || u8(bytes, at + 2) > 3 ||
            u8(bytes, at + 3) < 50 || secondary < -1 || u16(bytes, at + 6))
            throw Error("Invalid private pose mapping record");
        previous = key;
    }
    privatePoses_ = bytes;
}
int AnimationRoutes::privatePose(int source, int owner, int flags, int state) const {
    if (source < 0 || source > 255 || owner < 0 || owner > 255 || flags < 0 || flags > 3 ||
        state < 0 || state > 255) return -2;
    const quint32 key = (quint32(source) << 24) | (quint32(owner) << 16) |
                        (quint32(flags) << 8) | quint32(state);
    for (int at = 0; at < privatePoses_.size(); at += 8)
        if (u32(privatePoses_, at) == key) return qint16(u16(privatePoses_, at + 4));
    return -2;
}
QByteArray AnimationRoutes::decodedResource() const {
    if (!pending_.isEmpty())
        throw Error(QString("Class %1 needs an explicit animation source assignment before export")
                        .arg(*pending_.cbegin()));
    if (!modified())
        return source_;
    auto rows = rows_;
    rows.resize(rowCount());
    for (auto it = edits_.cbegin(); it != edits_.cend(); ++it)
        rows[it.key()] = it.value();
    return pack(prefix_, rows, privatePoses_);
}
QByteArray AnimationRoutes::section(int index) const {
    if (index < 0 || index >= 15)
        throw Error("Invalid compound animation section");
    const int begin = u16(prefix_, index * 2), end = sectionEnd(prefix_, begin);
    if (begin < 36 || end < begin || end > prefix_.size())
        throw Error("Compound animation section exceeds its source");
    return prefix_.mid(begin, end - begin);
}
QByteArray AnimationRoutes::rebuild(const Rom &rom, const QByteArray &compiled,
                                    const QByteArray &compiledCopyLoads) const {
    if (compiled != decodedResource())
        throw Error("Compiled class-animation data differ from the project");
    if (!modified())
        return rom.bytes();
    if (compiledCopyLoads.size() != 8 || u32(compiledCopyLoads, 0) != 0x9604001eU ||
        u32(compiledCopyLoads, 4) != 0x9606001eU)
        throw Error(
            "Compiled class-animation copy operands do not read the complete resource size");
    if (rom.bytes().mid(descriptorOffset, descriptorSize) != descriptor_ ||
        (!allocationOffset_ && descriptor_ != QByteArray(descriptorSize, char(-1))))
        throw Error("Class-animation descriptor is occupied or changed by another export owner");
    for (auto it = ownerWords_.cbegin(); it != ownerWords_.cend(); ++it)
        if (u32(rom.bytes(), it.key()) != it.value())
            throw Error("Another export owner changed a class-animation resource link");
    if (rawResource(rom, resourceKey(rom)) != source_)
        throw Error("Another export owner changed the class-animation compound resource");
    if (allocationOffset_ &&
        QByteArray::fromHex(sha256(rom.bytes().mid(allocationOffset_, allocationSize_))) !=
            descriptor_.mid(64, 32))
        throw Error("Another export owner changed the class-animation allocation");
    QByteArray payload(4, 0);
    put32(payload, 0, quint32(compiled.size()));
    payload += compiled;
    payload += QByteArray((16 - payload.size() % 16) % 16, char(-1));
    const bool tail = allocationOffset_ &&
                      quint64(allocationOffset_) + allocationSize_ == quint64(rom.bytes().size());
    const auto offset = allocationOffset_ && (payload.size() <= allocationSize_ || tail)
                            ? allocationOffset_
                            : (quint32(rom.bytes().size()) + 15) & ~15U;
    if (offset < 0x2800000 || quint64(offset) + payload.size() > 0x4000000)
        throw Error("Class-animation data exceed cartridge capacity");
    auto image = rom.bytes();
    if (allocationOffset_)
        image.replace(allocationOffset_, allocationSize_, QByteArray(allocationSize_, char(-1)));
    if (tail && offset == allocationOffset_)
        image.resize(offset);
    if (quint64(image.size()) < quint64(offset) + payload.size())
        image += QByteArray(offset + payload.size() - image.size(), char(-1));
    image.replace(offset, payload.size(), payload);
    const auto key = offset - resourceBase;
    for (const auto &[hi, lo] : sites()) {
        put32(image, hi, (ownerWords_[hi] & 0xffff0000U) | (key >> 16));
        put32(image, lo, (ownerWords_[lo] & 0xffff0000U) | (key & 65535));
    }
    for (const auto &[allocation, copy] : copySites()) {
        image.replace(allocation, 4, compiledCopyLoads.left(4));
        image.replace(copy, 4, compiledCopyLoads.mid(4));
    }
    QByteArray descriptor(descriptorSize, 0);
    descriptor.replace(0, 8, "LC05ARTE");
    put32(descriptor, 8, privatePoses_.isEmpty() ? 1 : 2);
    put32(descriptor, 12, offset);
    put32(descriptor, 16, quint32(payload.size()));
    put32(descriptor, 20, quint32(prefix_.size()));
    put32(descriptor, 24, quint32(rowCount()));
    descriptor.replace(28, 36, prefix_.left(36));
    descriptor.replace(64, 32, QByteArray::fromHex(sha256(payload)));
    image.replace(descriptorOffset, descriptorSize, descriptor);
    if (load(Rom::fromBytes(image)).source_ != compiled)
        throw Error("Class-animation resource failed readback");
    return image;
}
} // namespace lordly
