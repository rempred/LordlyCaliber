#include "engine/actor_art.h"
#include "engine/class_data.h"
#include "game/director/actor_state.h"
#include <iterator>
namespace lordly {
namespace {
QByteArray resource(const Rom &rom, quint32 key, bool compressed) {
    if (!key || key % 2 || key >= 0x10000000)
        throw Error("Invalid actor resource key");
    const quint32 at = key + 0x594280, size = u32(rom.bytes(), at);
    if (size > 16 * 1024 * 1024 || quint64(at) + 4 + size > quint64(rom.bytes().size()))
        throw Error("Actor resource exceeds its ROM envelope");
    if (!compressed)
        return rom.bytes().mid(at + 4, size);
    const auto decoded = u32(rom.bytes(), at + 4);
    if (size < 4 || decoded > 16 * 1024 * 1024)
        throw Error("Actor resource exceeds its decode budget");
    qsizetype used = 0;
    const auto data = decodeLzss(rom.bytes().mid(at + 8, size - 4), decoded, &used);
    if (used != size - 4)
        throw Error("Actor resource leaves unread compressed data");
    return data;
}
QVector<PoseProgram> poseTable(const QByteArray &bytes) {
    const auto first = u32(bytes, 0);
    if (!first || first % 4 || first > quint32(bytes.size()) || first / 4 > 32768)
        throw Error("Invalid actor pose directory extent");
    const int count = int(first / 4);
    QVector<quint32> offsets;
    for (int i = 0; i < count; ++i) {
        const auto offset = u32(bytes, i * 4);
        if (offset < first || offset > quint32(bytes.size()) || (i && offset < offsets.last()))
            throw Error("Invalid actor pose directory pointer");
        offsets << offset;
    }
    offsets << quint32(bytes.size());
    constexpr int widths[] = {1, 3, 3, 2, 2, 2, 1, 1, 1, 1, 1, 1, 4, 3, 3, 2, 3, 3, 3, 3, 3, 4};
    QVector<PoseProgram> result;
    for (int i = 0; i < count; ++i) {
        PoseProgram pose{bytes.mid(offsets[i], offsets[i + 1] - offsets[i]), {}};
        if (!pose.source.isEmpty()) {
            const int records = u8(pose.source, 0);
            int cursor = 1;
            for (int r = 0; r < records; ++r) {
                const int opcode = u8(pose.source, cursor);
                if (opcode >= int(std::size(widths)) ||
                    cursor + widths[opcode] > pose.source.size())
                    throw Error("Actor pose opcode exceeds its program");
                pose.records << PoseRecord{opcode, pose.source.mid(cursor + 1, widths[opcode] - 1)};
                cursor += widths[opcode];
            }
            if (cursor != pose.source.size())
                throw Error("Actor pose has unaccounted bytes");
        }
        result << std::move(pose);
    }
    return result;
}
} // namespace
int ActorArt::bankCount(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Actor art currently requires US revision 0");
    const auto directory = resource(rom, 0x0109f95e, false);
    if (directory.size() % 4 || directory.size() > 32768 * 4)
        throw Error("Invalid actor bank directory size");
    return int(directory.size() / 4);
}
ActorArt ActorArt::load(const Rom &rom, int bank) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Actor art currently requires US revision 0");
    // The separate cutscene root has one-based bank IDs. Combat bodies use a
    // different descriptor directory; do not reinterpret their class handles here.
    const auto directory = resource(rom, 0x0109f95e, false);
    if (directory.size() % 4 || bank < 1 || bank > directory.size() / 4)
        throw Error("Actor art bank is outside its current ROM directory");
    auto result = descriptor(rom, u32(directory, (bank - 1) * 4));
    result.bank_ = bank;
    // Both the original shared initializer and the bank-local resolver use
    // this vocabulary. It is allocated per scene but loaded from one ROM resource.
    const auto states = resource(rom, 0x0109eeb0, true);
    if (states.size() % 2)
        throw Error("Actor state vocabulary is not halfword aligned");
    for (int at = 0; at < states.size(); at += 2)
        result.stateTable_ << qint16(u16(states, at));
    return result;
}
ActorArt ActorArt::loadBody(const Rom &rom, int source, int owner, int flagA, int flagB) {
    const auto classes = ClassData::load(rom);
    if (source < 0 || source >= classes.entries().size() || owner < 0 || flagA < 0 || flagA > 1 ||
        flagB < 0 || flagB > 1)
        throw Error("Invalid class body source, owner or flags");
    // func_002015C8: byte at RAM index-base + 0x45, not modder body byte B45.
    const auto handles = resource(rom, 0x315736, false);
    const quint32 begin = u16(handles, 0), end = u16(handles, 2);
    if (begin > end || end > quint32(handles.size()) || (end - begin) % 2)
        throw Error("Class body route exceeds its current ROM handle table");
    QVector<unsigned short> table;
    for (quint32 at = begin; at < end; at += 2)
        table << u16(handles, at);
    unsigned short handle = 0;
    if (lordly_actor_body_handle(table.constData(), int(table.size()), source, owner,
                                 u8(classes.entries()[source].record, 0x45), flagA, flagB, &handle))
        throw Error("Class body route exceeds its current ROM handle table");
    const auto directory = resource(rom, 0x3b6cd0, false);
    const int slot = (handle & 0xfff) - 1;
    if (directory.size() % 4 || slot < 0 || slot >= directory.size() / 4)
        throw Error("Class body handle selects an absent descriptor");
    auto result = descriptor(rom, u32(directory, slot * 4));
    result.body_ = true;
    result.sourceClass_ = source;
    result.ownerClass_ = owner;
    result.flagA_ = flagA;
    result.flagB_ = flagB;
    result.bodyHandle_ = handle;
    return result;
}
ActorArt ActorArt::descriptor(const Rom &rom, quint32 key) {
    ActorArt result;
    result.descriptorKey_ = key;
    const auto descriptor = resource(rom, result.descriptorKey_, false);
    if (descriptor.size() < 16 || descriptor.size() % 4 || descriptor.size() > 65540 * 4)
        throw Error("Invalid actor art descriptor");
    result.metadata_ = resource(rom, u32(descriptor, 0), true);
    result.poses_ = poseTable(resource(rom, u32(descriptor, 4), true));
    const auto config = resource(rom, u32(descriptor, 8), true);
    result.palettes_ = resource(rom, u32(descriptor, 12), true);
    if (result.palettes_.size() % 512)
        throw Error("Actor palette banks are truncated");
    for (int at = 16; at < descriptor.size(); at += 4)
        result.artKeys_ << u32(descriptor, at);
    const auto map = u32(config, 0x1c);
    if (u32(config, 0) != result.artKeys_.size() ||
        quint64(map) + result.artKeys_.size() > quint64(config.size()))
        throw Error("Actor palette map does not match its art count");
    result.paletteMap_ = config.mid(map, result.artKeys_.size());
    return result;
}
int ActorArt::resolve(int animation, int facing, int option) const {
    int state = -1;
    if (lordly_actor_resolve_state(stateTable_.constData(), int(stateTable_.size()), bank_,
                                   animation, facing, option, &state))
        throw Error("Actor state lookup exceeds its current ROM vocabulary");
    if (state >= poses_.size())
        throw Error("Actor state vocabulary selects an absent pose program");
    return state;
}
QVector<ActorPoseSelection> ActorArt::selections() const {
    if (bank_ < 1 || (bank_ - 1) * 2 >= stateTable_.size())
        throw Error("Actor bank exceeds its state vocabulary");
    const int start = stateTable_[(bank_ - 1) * 2];
    QVector<ActorPoseSelection> result;
    if (start < 0 || start >= stateTable_.size())
        throw Error("Invalid actor state vocabulary pointer");
    for (int i = start; i < stateTable_.size(); ++i) {
        const int key = stateTable_[i];
        if (key == -1)
            return result;
        if (key < 0 || i - start >= poses_.size())
            throw Error("Invalid actor state vocabulary entry");
        result << ActorPoseSelection{key / 10, key % 10, i - start};
    }
    throw Error("Unterminated actor state vocabulary");
}
PoseRecord ActorArt::poseRecord(int state, int cursor) const {
    if (state < 0 || state >= poses_.size())
        throw Error("Actor pose state is outside its directory");
    // Equal directory offsets alias the following counted program. Preserve
    // their empty physical slices in poses(), but follow the pointer at runtime.
    while (state < poses_.size() && poses_[state].source.isEmpty())
        ++state;
    if (state >= poses_.size())
        throw Error("Actor pose pointer is past its resource");
    const auto &records = poses_[state].records;
    return cursor < 0 || cursor >= records.size() ? PoseRecord{} : records[cursor];
}
QVector<ActorLayer> ActorArt::layers(int token) const {
    if (token < 0 || token > 65535)
        throw Error("Actor frame token is out of range");
    const auto offset = u32(metadata_, token * 4);
    const auto count = u16(metadata_, offset);
    if (count > 4096 || quint64(offset) + 2 + count * 16 > quint64(metadata_.size()))
        throw Error("Actor frame layers exceed their metadata resource");
    QVector<ActorLayer> result;
    for (int i = 0; i < count; ++i) {
        const auto at = offset + 2 + i * 16;
        ActorLayer layer{u16(metadata_, at),
                         {qint16(u16(metadata_, at + 2)), qint16(u16(metadata_, at + 4))},
                         u16(metadata_, at + 6),
                         u16(metadata_, at + 8),
                         u16(metadata_, at + 10),
                         u16(metadata_, at + 12),
                         u16(metadata_, at + 14)};
        if (layer.art >= artKeys_.size() || !layer.width || !layer.height ||
            quint64(layer.width) * layer.height > 4194304)
            throw Error("Actor frame selects invalid art or dimensions");
        result << layer;
    }
    return result;
}
QVector<ActorFrameLayer> ActorArt::frame(const Rom &rom, int token, int variant, bool sepia) const {
    QVector<ActorFrameLayer> result;
    qsizetype retained = 0;
    for (const auto &layer : layers(token)) {
        const auto sprite = SpriteObject::decode(resource(rom, artKeys_[layer.art], true));
        const auto palette = palettes_.mid(u8(paletteMap_, layer.art) * 512, 512);
        auto pixels = sprite.frame(variant, palette, sepia);
        pixels = pixels.oriented(layer.width, layer.height, layer.flags & 1, layer.flags & 2);
        if ((retained += pixels.image.rgba.size() + pixels.colorAlpha.size()) > 64 * 1024 * 1024)
            throw Error("Actor frame exceeds the pixel budget");
        result << ActorFrameLayer{layer, std::move(pixels)};
    }
    return result;
}
} // namespace lordly
