#include "engine/animation_sequences.h"
#include "engine/body_markers.h"
#include <QJsonArray>
namespace lordly {
namespace {
int groupId(int classId, int flags) {
    if (classId < 1 || classId > 255 || flags < 0 || flags > 3)
        throw Error("Invalid private animation class or body route");
    return classId * 4 + flags;
}
QByteArray container(const QByteArray &decoded) {
    QByteArray result(8, 0);
    const auto compressed = encodeLzss(decoded);
    put32(result, 0, quint32(compressed.size() + 4));
    put32(result, 4, quint32(decoded.size()));
    return result + compressed;
}
void pad(QByteArray &bytes) {
    bytes += QByteArray((16 - bytes.size() % 16) % 16, char(-1));
}
QByteArray baselineBytes(const QJsonObject &baselines) {
    if (baselines.size() > 128)
        throw Error("Too many private attack assignments");
    QMap<int, QJsonValue> sorted;
    for (auto it = baselines.begin(); it != baselines.end(); ++it) {
        bool ok = false;
        const int action = it.key().toInt(&ok);
        if (!ok || action < 0 || action > 255 || QString::number(action) != it.key())
            throw Error("Invalid private attack identity");
        sorted[action] = it.value();
    }
    QByteArray bytes;
    for (auto it = sorted.begin(); it != sorted.end(); ++it) {
        QByteArray row(8, 0);
        put8(row, 0, quint8(it.key()));
        if (!it->isNull()) {
            const auto value = it->toObject();
            if (!it->isObject() || value.size() != 2)
                throw Error("Invalid prior private attack assignment");
            put8(row, 1, 1);
            for (const auto &[key, at] :
                 {QPair<const char *, int>{"normalSelector", 2}, {"blockedSelector", 3}}) {
                const auto number = value[key].toInteger(-1);
                if (number < 0 || number > 255 || value[key].toDouble() != double(number))
                    throw Error("Invalid prior private attack selector");
                put8(row, at, quint8(number));
            }
        }
        bytes += row;
    }
    return bytes;
}
QJsonObject readBaselines(const QByteArray &bytes) {
    if (bytes.isEmpty() || bytes.size() % 8 || bytes.size() > 128 * 8)
        throw Error("Invalid private attack baseline resource");
    QJsonObject result;
    int previous = -1;
    for (int at = 0; at < bytes.size(); at += 8) {
        const int action = u8(bytes, at), exact = u8(bytes, at + 1);
        if (action <= previous || exact > 1 || u32(bytes, at + 4) || (!exact && u16(bytes, at + 2)))
            throw Error("Invalid private attack baseline record");
        previous = action;
        result[QString::number(action)] =
            exact ? QJsonValue(QJsonObject{{"normalSelector", u8(bytes, at + 2)},
                                           {"blockedSelector", u8(bytes, at + 3)}})
                  : QJsonValue(QJsonValue::Null);
    }
    return result;
}
QByteArray pairingBytes(const AnimationSequences::Group &group) {
    const int start = group.reservedAttackStart;
    if ((start && (start < 101 || start > 256)) || group.pairedPoses.size() > 156)
        throw Error("Invalid reserved private attack range");
    QByteArray bytes;
    for (auto it = group.pairedPoses.begin(); it != group.pairedPoses.end(); ++it) {
        const int primary = it.key(), secondary = it.value();
        if (!start || primary < 100 || primary > 255 || secondary < 256 || secondary > 32767 ||
            secondary >= group.descriptor.poses().programs().size())
            throw Error("Invalid private secondary pose");
        group.descriptor.poses().validateEntry(primary);
        group.descriptor.poses().validateEntry(secondary);
        QByteArray row(4, 0);
        put16(row, 0, quint16(primary)); put16(row, 2, quint16(secondary));
        bytes += row;
    }
    if (start && group.descriptor.poses().programs().size() < 256)
        throw Error("Reserved private attacks exceed their pose bank");
    return bytes;
}
} // namespace
AnimationSequences AnimationSequences::load(const Rom &rom) {
    AnimationSequences result;
    result.rom_ = rom;
    result.header_ = rom.bytes().mid(descriptorOffset, descriptorSize);
    const auto &d = result.header_;
    if (d.left(8) != "LC05ASEQ") {
        result.original_ = result.json();
        return result;
    }
    if (d.size() != descriptorSize || (u32(d, 8) < 1 || u32(d, 8) > 3) ||
        d.mid(24, 40) != QByteArray(40, 0))
        throw Error("Invalid private animation allocation header");
    result.offset_ = u32(d, 12);
    result.size_ = u32(d, 16);
    const auto count = u32(d, 20);
    if (!count || count > 1020 || result.offset_ < 0x2800000 || result.offset_ % 16 ||
        !result.size_ || result.size_ % 16 ||
        quint64(result.offset_) + result.size_ > quint64(rom.bytes().size()) ||
        quint64(count) * 16 > result.size_)
        throw Error("Private animation allocation has invalid bounds");
    const auto data = rom.bytes().mid(result.offset_, result.size_);
    if (QByteArray::fromHex(sha256(data)) != d.mid(64, 32))
        throw Error("Private animation allocation identity differs");
    const auto directory = DirectDescriptors::load(rom).keys();
    QSet<int> handles;
    QMap<quint32, quint32> spans;
    auto span = [&](quint32 key) {
        const quint64 at = quint64(key) + DirectDescriptors::resourceBase;
        if (!key || key % 2 || at < result.offset_ + count * 16 ||
            at + 4 > quint64(result.offset_) + result.size_)
            throw Error("Private animation member leaves its owned allocation");
        const auto length = u32(rom.bytes(), qsizetype(at));
        if (length > 16 * 1024 * 1024 || at + 4 + length > quint64(result.offset_) + result.size_)
            throw Error("Private animation member exceeds its allocation");
        spans[quint32(at) - result.offset_] = length + 4;
    };
    int previous = -1;
    quint64 decoded = 0;
    for (quint32 i = 0; i < count; ++i) {
        const int id = groupId(u16(data, i * 16), u16(data, i * 16 + 2));
        Group group;
        group.baselineHandle = u16(data, i * 16 + 4);
        group.privateHandle = u16(data, i * 16 + 6);
        const auto key = u32(data, i * 16 + 8);
        const int slot = (group.privateHandle & 4095) - 1, base = (group.baselineHandle & 4095) - 1;
        if (id <= previous || (u32(d, 8) == 1 && u32(data, i * 16 + 12)) || slot < 0 ||
            slot >= directory.size() || base < 0 || base >= directory.size() || !directory[base] ||
            directory[slot] != key || handles.contains(slot) ||
            (group.privateHandle & 0xf000) != (group.baselineHandle & 0xf000))
            throw Error("Private animation class, handle or descriptor references differ");
        previous = id;
        handles.insert(slot);
        span(key);
        if (const auto baseline = u32(data, i * 16 + 12)) {
            span(baseline);
            const auto at = baseline + DirectDescriptors::resourceBase;
            auto metadata = rom.bytes().mid(at + 4, u32(rom.bytes(), at));
            if (u32(d, 8) == 3) {
                const int baselineSize = u16(metadata, 0) * 8;
                group.reservedAttackStart = u16(metadata, 2);
                if (metadata.size() < 4 + baselineSize || (metadata.size() - 4 - baselineSize) % 4)
                    throw Error("Invalid private pairing resource");
                int previousPair = -1;
                for (int pos = 4 + baselineSize; pos < metadata.size(); pos += 4) {
                    const int primary = u16(metadata, pos);
                    if (primary <= previousPair) throw Error("Unsorted private pose pairs");
                    previousPair = primary;
                    group.pairedPoses[primary] = u16(metadata, pos + 2);
                }
                metadata = metadata.mid(4, baselineSize);
            }
            if (!metadata.isEmpty()) group.attackBaselines = readBaselines(metadata);
        }
        group.descriptor = AnimationDescriptor::load(rom, key);
        for (int m = 0; m < group.descriptor.members().size(); ++m) {
            const auto member = group.descriptor.members()[m];
            if (!member && m >= 4)
                continue;
            span(member);
            decoded += m < 4 ? group.descriptor.controls()[m].size()
                             : group.descriptor.sprite(m - 4).size();
            if (decoded > 512ULL * 1024 * 1024)
                throw Error("Private animations exceed their decoded data budget");
        }
        pairingBytes(group);
        result.groups_[id] = std::move(group);
    }
    for (const auto &group : result.groups_)
        if (handles.contains((group.baselineHandle & 4095) - 1))
            throw Error("Private animation baseline refers to another private owner");
    quint32 cursor = count * 16;
    for (auto it = spans.cbegin(); it != spans.cend(); ++it) {
        if (it.key() != cursor)
            throw Error("Private animation allocation contains overlapping or unclaimed data");
        const quint32 end = cursor + it.value();
        cursor = (end + 15) & ~15U;
        if (data.mid(end, cursor - end) != QByteArray(cursor - end, char(-1)))
            throw Error("Private animation padding differs");
    }
    if (cursor != result.size_)
        throw Error("Private animation allocation has an unclaimed tail");
    result.loaded_ = result.groups_;
    if (result.requiresMarkerRuntime() && BodyMarkerImage::mode(rom) != 1)
        throw Error("Private wide-frame equipment markers need their source-built runtime; its "
                    "code is absent or changed");
    result.original_ = result.json();
    return result;
}
QSet<quint32> AnimationSequences::ownedDescriptors(const Rom &rom) {
    QSet<quint32> result;
    const auto sequences = load(rom);
    for (const auto &group : sequences.groups_)
        result.insert(group.descriptor.key());
    return result;
}
const AnimationDescriptor *AnimationSequences::find(int classId, int flags) const {
    const auto it = groups_.constFind(groupId(classId, flags));
    return it == groups_.cend() ? nullptr : &it->descriptor;
}
void AnimationSequences::create(int classId, int flags, const AnimationRoutes &routes,
                                const ActorArtworkEdits &artwork) {
    const int id = groupId(classId, flags);
    if (groups_.contains(id))
        return;
    const auto handle = routes.row(classId)[flags];
    const auto root = DirectDescriptors::load(rom_).keys();
    const int slot = (handle & 4095) - 1;
    if (slot < 0 || slot >= root.size() || !root[slot])
        throw Error("The private sequence needs a populated body route");
    Group group{handle, 0, AnimationDescriptor::load(rom_, root[slot], artwork.value(root[slot]))};
    if (loaded_.contains(id))
        group.privateHandle = loaded_[id].privateHandle;
    groups_[id] = std::move(group);
}
void AnimationSequences::setDescriptor(int classId, int flags, const AnimationDescriptor &value) {
    const int id = groupId(classId, flags);
    auto it = groups_.find(id);
    if (it == groups_.end() || it->descriptor.key() != value.key())
        throw Error("Create a private copy before editing its descriptor");
    it->descriptor = value;
}
void AnimationSequences::rememberAttackBaseline(int classId, int flags, int action,
                                                const QJsonValue &baseline) {
    const int id = groupId(classId, flags);
    auto it = groups_.find(id);
    if (it == groups_.end())
        throw Error("Create a private copy before assigning its attacks");
    auto values = it->attackBaselines;
    const auto key = QString::number(action);
    if (values.contains(key))
        return;
    values[key] = baseline;
    baselineBytes(values);
    it->attackBaselines = values;
}
void AnimationSequences::setPairedPoses(int classId, int flags, const QMap<int, int> &pairs, int start) {
    auto it = groups_.find(groupId(classId, flags));
    if (it == groups_.end()) throw Error("Create a private copy before pairing attacks");
    auto next = *it;
    next.pairedPoses = pairs; next.reservedAttackStart = start;
    pairingBytes(next);
    *it = std::move(next);
}
void AnimationSequences::clearAttackBaseline(int classId, int flags, int action) {
    auto it = groups_.find(groupId(classId, flags));
    if (it != groups_.end())
        it->attackBaselines.remove(QString::number(action));
}
void AnimationSequences::remove(int classId, int flags) {
    groups_.remove(groupId(classId, flags));
}
void AnimationSequences::cloneClass(int source, int target) {
    auto next = groups_;
    for (int flag = 0; flag < 4; ++flag) {
        const int id = groupId(source, flag), to = groupId(target, flag);
        if (groups_.contains(to))
            throw Error("The new class already has a private animation");
        if (groups_.contains(id)) {
            auto copy = groups_[id];
            copy.privateHandle = 0;
            next[to] = std::move(copy);
        }
    }
    groups_ = std::move(next);
}
AnimationRoutes AnimationSequences::baseRoutes(AnimationRoutes routes) const {
    for (auto it = loaded_.cbegin(); it != loaded_.cend(); ++it) {
        auto row = routes.row(it.key() / 4);
        if (row[it.key() % 4] != it->privateHandle)
            throw Error("Private animation body assignment no longer matches its owner");
        row[it.key() % 4] = it->baselineHandle;
        routes.setRow(it.key() / 4, row);
    }
    return routes;
}
void AnimationSequences::validate(const AnimationRoutes &routes, int count) const {
    QSet<int> privateHandles;
    for (const auto &group : loaded_)
        privateHandles.insert(group.privateHandle & 4095);
    for (auto it = groups_.cbegin(); it != groups_.cend(); ++it)
        if (it.key() / 4 >= count || routes.row(it.key() / 4)[it.key() % 4] != it->baselineHandle ||
            privateHandles.contains(it->baselineHandle & 4095))
            throw Error("Remove the private animation copy before changing its body route");
    for (int id = 0; id < routes.rowCount(); ++id) {
        if (routes.pending().contains(id))
            continue;
        for (auto handle : routes.row(id))
            if (privateHandles.contains(handle & 4095))
                throw Error(
                    "A private animation handle cannot be shared through another body assignment");
    }
    if (requiresMarkerRuntime() && BodyMarkerImage::mode(rom_) < 0)
        throw Error(
            "The equipment marker code is not recognized and cannot accept these marker edits");
}
bool AnimationSequences::requiresMarkerRuntime() const {
    for (const auto &group : groups_) {
        const auto bank = group.descriptor.poses();
        if (bank.programs().size() < 39 || bank.programs().size() == 88)
            return true;
        for (int state : {38, 88}) {
            if (state >= bank.programs().size())
                continue;
            while (state < bank.programs().size() && bank.programs()[state].source.isEmpty())
                ++state;
            if (state >= bank.programs().size())
                continue;
            for (const auto &record : bank.programs()[state].records)
                if (record.opcode == 21)
                    return true;
        }
    }
    return false;
}
QJsonObject AnimationSequences::json() const {
    QJsonArray rows;
    for (auto it = groups_.cbegin(); it != groups_.cend(); ++it) {
        QJsonObject row{{"classId", it.key() / 4},
                        {"bodyFlags", it.key() % 4},
                        {"baselineHandle", it->baselineHandle},
                        {"descriptor", it->descriptor.json()}};
        if (!it->attackBaselines.isEmpty())
            row["attackBaselines"] = it->attackBaselines;
        if (it->reservedAttackStart) {
            row["reservedAttackStart"] = it->reservedAttackStart;
            QJsonObject pairs;
            for (auto pair = it->pairedPoses.begin(); pair != it->pairedPoses.end(); ++pair)
                pairs[QString::number(pair.key())] = pair.value();
            row["pairedPoses"] = pairs;
        }
        rows << row;
    }
    return {{"version", 1}, {"groups", rows}};
}
void AnimationSequences::apply(const QJsonObject &object) {
    if (object.size() != 2 || object["version"].toInt() != 1 || !object["groups"].isArray() ||
        object["groups"].toArray().size() > 1020)
        throw Error("Invalid saved private sequences");
    QMap<int, Group> groups;
    for (const auto &value : object["groups"].toArray()) {
        const auto row = value.toObject();
        if (!value.isObject() || row.size() != 4 + int(row.contains("attackBaselines")) +
                2 * int(row.contains("reservedAttackStart")) ||
            (row.contains("attackBaselines") && !row["attackBaselines"].isObject()) ||
            !row["descriptor"].isObject())
            throw Error("Invalid saved private sequence");
        const auto integer = [&](const char *key) {
            const auto number = row[key].toInteger(-1);
            if (number < 0 || number > 65535 || row[key].toDouble() != double(number))
                throw Error("Invalid private sequence field");
            return int(number);
        };
        const int id = groupId(integer("classId"), integer("bodyFlags")),
                  base = integer("baselineHandle");
        if (groups.contains(id) || !(base & 4095))
            throw Error("Duplicate private sequence or absent base handle");
        groups[id] = {quint16(base), loaded_.contains(id) ? loaded_[id].privateHandle : quint16(0),
                      AnimationDescriptor::fromJson(rom_, row["descriptor"].toObject())};
        groups[id].attackBaselines = row["attackBaselines"].toObject();
        baselineBytes(groups[id].attackBaselines);
        if (row.contains("reservedAttackStart")) {
            groups[id].reservedAttackStart = integer("reservedAttackStart");
            if (!groups[id].reservedAttackStart || !row["pairedPoses"].isObject())
                throw Error("Invalid saved private pose pairs");
            const auto pairs = row["pairedPoses"].toObject();
            for (auto it = pairs.begin(); it != pairs.end(); ++it) {
                bool ok = false;
                const int primary = it.key().toInt(&ok), secondary = it->toInt(-1);
                if (!ok || QString::number(primary) != it.key() || it->toDouble() != double(secondary))
                    throw Error("Invalid saved private pose pair");
                groups[id].pairedPoses[primary] = secondary;
            }
        }
        pairingBytes(groups[id]);
    }
    groups_ = std::move(groups);
}
AnimationSequences::Plan AnimationSequences::plan(const Rom &rom) const {
    if (rom.bytes().mid(descriptorOffset, descriptorSize) != header_ ||
        (!offset_ && header_ != QByteArray(descriptorSize, char(-1))))
        throw Error("Private animation header is occupied or changed by another owner");
    if (offset_ && rom.bytes().mid(offset_, size_) != rom_.bytes().mid(offset_, size_))
        throw Error("Private animation allocation changed by another owner");
    Plan result;
    result.directory = DirectDescriptors::load(rom);
    for (const auto &group : loaded_) {
        const int handle = group.privateHandle & 4095;
        if (handle > result.directory.keys().size() ||
            result.directory.keys()[handle - 1] != group.descriptor.key())
            throw Error("Private descriptor directory changed by another owner");
        result.directory.set(handle, 0);
    }
    for (const auto &group : groups_)
        if (group.privateHandle)
            result.directory.set(group.privateHandle & 4095, group.descriptor.key());
    for (auto it = groups_.cbegin(); it != groups_.cend(); ++it) {
        int handle = it->privateHandle & 4095;
        if (!handle) {
            // Reuse cleared expansion entries. Retail's reserved slots remain intact.
            for (int slot = 208; slot < result.directory.keys().size(); ++slot)
                if (!result.directory.keys()[slot]) {
                    handle = slot + 1;
                    break;
                }
            if (handle)
                result.directory.set(handle, it->descriptor.key());
            else
                handle = result.directory.append(it->descriptor.key());
        }
        result.handles[it.key()] = quint16((it->baselineHandle & 0xf000) | handle);
    }
    if (groups_.isEmpty())
        return result;
    QMap<int, QVector<QByteArray>> containers;
    for (auto it = groups_.cbegin(); it != groups_.cend(); ++it) {
        const auto &descriptor = it->descriptor;
        const auto original = AnimationDescriptor::load(rom_, descriptor.key());
        auto &members = containers[it.key()];
        for (int member = 0; member < descriptor.members().size(); ++member) {
            const auto key = descriptor.members()[member];
            if (member >= 4 && !key && !descriptor.artwork().contains(member - 4)) {
                members << QByteArray{};
                continue;
            }
            if (member < 4 && descriptor.controls()[member] != original.controls()[member])
                members << container(descriptor.controls()[member]);
            else if (member >= 4 && descriptor.artwork().contains(member - 4))
                members << container(descriptor.artwork().value(member - 4));
            else {
                const quint64 at = quint64(key) + DirectDescriptors::resourceBase;
                const auto size = u32(rom_.bytes(), qsizetype(at));
                if (size > 16 * 1024 * 1024 || at + 4 + size > quint64(rom_.bytes().size()))
                    throw Error("Private sprite container exceeds its source ROM");
                members << rom_.bytes().mid(qsizetype(at), size + 4);
            }
        }
    }
    bool paired = false;
    for (const auto &group : groups_) paired |= bool(group.reservedAttackStart);
    const auto build = [&](quint32 offset) {
        QByteArray bytes(groups_.size() * 16, 0);
        QMap<QByteArray, quint32> resources;
        const auto append = [&](const QByteArray &resource) {
            if (resources.contains(resource))
                return resources.value(resource);
            const quint32 key = offset + quint32(bytes.size()) - DirectDescriptors::resourceBase;
            resources[resource] = key;
            bytes += resource;
            pad(bytes);
            return key;
        };
        int row = 0;
        for (auto it = groups_.cbegin(); it != groups_.cend(); ++it, ++row) {
            const auto &descriptor = it->descriptor;
            QByteArray members(descriptor.members().size() * 4, 0);
            for (int member = 0; member < descriptor.members().size(); ++member) {
                if (member >= 4 && !descriptor.members()[member] &&
                    !descriptor.artwork().contains(member - 4))
                    continue;
                put32(members, member * 4, append(containers[it.key()][member]));
            }
            QByteArray raw(4, 0);
            put32(raw, 0, quint32(members.size()));
            raw += members;
            const auto key = offset + quint32(bytes.size()) - DirectDescriptors::resourceBase;
            bytes += raw;
            pad(bytes);
            put16(bytes, row * 16, quint16(it.key() / 4));
            put16(bytes, row * 16 + 2, quint16(it.key() % 4));
            put16(bytes, row * 16 + 4, it->baselineHandle);
            put16(bytes, row * 16 + 6, result.handles[it.key()]);
            put32(bytes, row * 16 + 8, key);
            auto baselines = baselineBytes(it->attackBaselines);
            if (paired && (!baselines.isEmpty() || it->reservedAttackStart)) {
                QByteArray head(4, 0);
                put16(head, 0, quint16(baselines.size() / 8));
                put16(head, 2, quint16(it->reservedAttackStart));
                baselines = head + baselines + pairingBytes(*it);
            }
            if (!baselines.isEmpty()) {
                QByteArray resource(4, 0);
                put32(resource, 0, quint32(baselines.size()));
                resource += baselines;
                put32(bytes, row * 16 + 12, append(resource));
            }
            result.directory.set(result.handles[it.key()] & 4095, key);
            if (bytes.size() > 0x1800000)
                throw Error("Private animation payload exceeds cartridge capacity");
        }
        return bytes;
    };
    result.payload = build(0x2800000);
    const bool tail = offset_ && quint64(offset_) + size_ == quint64(rom.bytes().size());
    result.offset = offset_ && (result.payload.size() <= size_ || tail)
                        ? offset_
                        : (quint32(rom.bytes().size()) + 15) & ~15U;
    if (result.offset < 0x2800000 || quint64(result.offset) + result.payload.size() > 0x4000000)
        throw Error("Private animations exceed cartridge capacity");
    if (result.offset != 0x2800000)
        result.payload = build(result.offset);
    return result;
}
QByteArray AnimationSequences::materialize(const Rom &rom, const Plan &plan,
                                           const QByteArray &compiled) const {
    if (compiled != plan.payload)
        throw Error("Compiled private animations differ from the project");
    if (rom.bytes().mid(descriptorOffset, descriptorSize) != header_ ||
        (!offset_ && header_ != QByteArray(descriptorSize, char(-1))) ||
        (offset_ && rom.bytes().mid(offset_, size_) != rom_.bytes().mid(offset_, size_)))
        throw Error("Private animations changed before materialization");
    const bool tail = offset_ && quint64(offset_) + size_ == quint64(rom.bytes().size());
    const auto expected = compiled.isEmpty() ? 0U
                          : offset_ && (compiled.size() <= size_ || tail)
                              ? offset_
                              : (quint32(rom.bytes().size()) + 15) & ~15U;
    if (plan.offset != expected || compiled.isEmpty() != groups_.isEmpty() ||
        plan.handles.size() != groups_.size())
        throw Error("Private animation destination changed before materialization");
    auto image = rom.bytes();
    if (offset_)
        image.replace(offset_, size_, QByteArray(size_, char(-1)));
    if (offset_ && quint64(offset_) + size_ == quint64(image.size()))
        image.resize(offset_);
    QByteArray header(descriptorSize, char(-1));
    if (!compiled.isEmpty()) {
        if (plan.offset < 0x2800000 || quint64(plan.offset) + compiled.size() > 0x4000000)
            throw Error("Invalid private animation placement");
        if (image.size() < plan.offset + compiled.size())
            image += QByteArray(plan.offset + compiled.size() - image.size(), char(-1));
        image.replace(plan.offset, compiled.size(), compiled);
        header.fill(0);
        header.replace(0, 8, "LC05ASEQ");
        int version = 1;
        for (const auto &group : groups_) {
            if (!group.attackBaselines.isEmpty()) version = std::max(version, 2);
            if (group.reservedAttackStart) version = 3;
        }
        put32(header, 8, quint32(version));
        put32(header, 12, plan.offset);
        put32(header, 16, quint32(compiled.size()));
        put32(header, 20, quint32(groups_.size()));
        header.replace(64, 32, QByteArray::fromHex(sha256(compiled)));
    }
    image.replace(descriptorOffset, descriptorSize, header);
    return image;
}
AnimationRoutes AnimationSequences::exportRoutes(AnimationRoutes routes, const Plan &plan) const {
    routes = routes.withDirectory(plan.directory.decoded());
    for (auto it = plan.handles.cbegin(); it != plan.handles.cend(); ++it) {
        auto row = routes.row(it.key() / 4);
        row[it.key() % 4] = it.value();
        routes.setRow(it.key() / 4, row);
    }
    return routes;
}
} // namespace lordly
