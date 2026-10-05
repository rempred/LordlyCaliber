#include "engine/direct_descriptors.h"
#include "engine/actor_art.h"
#include "engine/body_markers.h"
#include "engine/class_data.h"
#include "engine/animation_routes.h"
#include "game/director/actor_body.h"
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
ActorArt ActorArt::loadBody(const Rom &rom, int source, int owner, int flagA, int flagB,
                            BodyAnimationData::Pointer data) {
    if (!data) data = BodyAnimationData::load(rom);
    const auto handle = data->handle(source, owner, flagA, flagB);
    auto result = loadDirect(rom, handle);
    result.bodyData_ = std::move(data);
    result.body_ = true;
    result.sourceClass_ = source;
    result.ownerClass_ = owner;
    result.flagA_ = flagA;
    result.flagB_ = flagB;
    result.bodyHandle_ = handle;
    result.wideBodyMarkers_=BodyMarkerImage::mode(rom)==1;
    result.prepareBodyFrames();
    if(const auto *privateDescriptor=result.bodyData_->descriptor(source,owner,flagA,flagB))
        return result.withDescriptor(*privateDescriptor);
    return result;
}
void ActorArt::prepareBodyFrames() {
    bodyReplacements_.fill(65535);
    // Accepted 205C88 scans pose 38/88 for the last two frame commands.
    for (int pair = 0; pair < 2; ++pair) {
        int state = pair ? 88 : 38;
        while (state < poses_.size() && poses_[state].source.isEmpty())
            ++state;
        if (state >= poses_.size())
            continue;
        for (const auto &command : poses_[state].records) {
            if (command.opcode == 1 || (wideBodyMarkers_&&command.opcode==21)) {
                bodyReplacements_[pair * 2 + 1] = bodyReplacements_[pair * 2];
                bodyReplacements_[pair * 2] = u8(command.operands, 0)+(command.opcode==21?256*u8(command.operands,1):0);
            }
        }
    }
}
ActorArt ActorArt::withDescriptor(const AnimationDescriptor &descriptor)const {
    auto result=*this;
    result.descriptorKey_=descriptor.key();
    result.metadata_=descriptor.controls()[0];
    result.poses_=descriptor.poses().programs();
    const auto &config=descriptor.controls()[2];
    result.palettes_=descriptor.controls()[3];
    result.artKeys_=descriptor.members().sliced(4);
    const auto map=u32(config,0x1c);
    result.paletteMap_=config.mid(map,result.artKeys_.size());
    result.imageKinds_=config.mid(0x20,result.artKeys_.size());
    result.privateArtwork_=descriptor.artwork();
    result.contentIdentity_=descriptor.identity();
    result.isolated_=true;
    result.wideBodyMarkers_=true;
    if(result.body_)result.prepareBodyFrames();
    return result;
}
ActorArt ActorArt::loadDirect(const Rom &rom, quint16 handle) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Direct descriptor art currently requires US revision 0");
    const auto directory = DirectDescriptors::directory(rom);
    const int slot = (handle & 0xfff) - 1;
    if (directory.size() % 4 || slot < 0 || slot >= directory.size() / 4)
        throw Error("Direct handle selects an absent descriptor");
    return descriptor(rom, u32(directory, slot * 4));
}
ActorArt ActorArt::descriptor(const Rom &rom, quint32 key) {
    ActorArt result;
    result.descriptorKey_ = key;
    const auto descriptor = resource(rom, result.descriptorKey_, false);
    if (descriptor.size() < 16 || descriptor.size() % 4 || descriptor.size() > 65540 * 4)
        throw Error("Invalid actor art descriptor");
    result.metadata_ = resource(rom, u32(descriptor, 0), true);
    result.poses_ = PoseBank::parse(resource(rom, u32(descriptor, 4), true)).programs();
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
    if (quint64(0x20) + result.artKeys_.size() > quint64(config.size()))
        throw Error("Actor image-kind map is truncated");
    result.imageKinds_ = config.mid(0x20, result.artKeys_.size());
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
QVector<ActorFrameLayer> ActorArt::frame(const Rom &rom, int token, int variant, int sepia,
                                         const QVector<quint8> &perLayer,
                                         const QMap<int, QByteArray> &artwork) const {
    QVector<ActorFrameLayer> result;
    qsizetype retained = 0;
    const auto metadata = layers(token);
    const auto &currentArtwork = isolated_ ? privateArtwork_ : artwork;
    if (!perLayer.isEmpty() && perLayer.size() != metadata.size())
        throw Error("Actor processing decisions do not match its layers");
    for (const auto &layer : metadata) {
        const auto bytes = currentArtwork.contains(layer.art) ? currentArtwork.value(layer.art)
                                                       : resource(rom, artKeys_[layer.art], true);
        SpriteFrame pixels;
        if (u16(bytes, 0) == 0x3634) {
            if (u8(bytes, 2) != 4)
                throw Error("Direct native image needs its texture layout conversion");
            pixels.image = decodeN64Image(bytes);
            pixels.sourceColorFormat = 4;
            pixels.requestedChild = variant;
            pixels.colorAlpha.resize(pixels.image.width * pixels.image.height);
            for (int i = 0; i < pixels.colorAlpha.size(); ++i) {
                // N64 I4/I8 supplies intensity in all four texture lanes.
                const auto intensity = u8(pixels.image.rgba, i * 4);
                put8(pixels.image.rgba, i * 4 + 3, intensity);
                put8(pixels.colorAlpha, i, intensity);
            }
        } else {
            const auto sprite = SpriteObject::decode(bytes);
            const auto palette = palettes_.mid(u8(paletteMap_, layer.art) * 512, 512);
            pixels = sprite.frame(variant, palette,
                                  perLayer.isEmpty() ? sepia : perLayer[result.size()]);
        }
        pixels = pixels.oriented(layer.width, layer.height, layer.flags & 1, layer.flags & 2);
        if ((retained += pixels.image.rgba.size() + pixels.colorAlpha.size()) > 64 * 1024 * 1024)
            throw Error("Actor frame exceeds the pixel budget");
        result << ActorFrameLayer{layer, std::move(pixels)};
    }
    return result;
}
QVector<BodyLayerSelection> ActorArt::bodyLayers(const Rom &rom, int token,
                                                 BodyAppearance appearance) const {
    if (!body_)
        throw Error("Equipment image selection requires class-body art");
    quint16 extraFlags = 0;
    if (token == bodyReplacements_[0]) {
        token = bodyReplacements_[1];
        extraFlags = 0x40;
    } else if (token == bodyReplacements_[2]) {
        token = bodyReplacements_[3];
        extraFlags = 0x80;
    }
    const int item = lordly_body_equipment_variant(sourceClass_, appearance.equipment);
    if (item < 0 || item >= 278)
        throw Error("Body equipment exceeds the current item table");
    const auto variants = bodyData_->routes().section(2);
    if (item >= variants.size())
        throw Error("Body equipment image variant exceeds its table");
    QVector<BodyLayerSelection> result;
    for (auto layer : layers(token)) {
        layer.flags |= extraFlags;
        int kind = u8(imageKinds_, layer.art);
        if ((sourceClass_ == 0x39 || sourceClass_ == 0x3f) && kind == 2)
            kind = 1;
        int child = bodyVariant(), palette = u8(paletteMap_, layer.art) * 512;
        if (kind == 1)
            child = u8(variants, item);
        else if (kind >= 2) {
            int selected = u8(rom.bytes(), 0x62300 + item * 32 + 0x11);
            if (selected == 16) {
                if (!appearance.paletteReplacement)
                    throw Error("Body equipment needs the launch palette replacement");
                selected = *appearance.paletteReplacement;
            }
            child = 0;
            palette += selected * 32;
        }
        if (palette < 0 || palette >= palettes_.size())
            throw Error("Body equipment palette exceeds its resource");
        result << BodyLayerSelection{layer, child, palette};
    }
    return result;
}
BodyAppearance ActorArt::defaultAppearance(const Rom &rom) const {
    if (!body_)
        throw Error("Class appearance requires body art");
    const auto &classes = bodyData_->classes();
    QByteArray records;
    for (const auto &entry : classes.entries())
        records += entry.record;
    const auto items = rom.bytes().mid(0x62300, 278 * 32);
    uint16_t carried[4], equipment;
    if (items.size() != 278 * 32 ||
        lordly_body_appearance(reinterpret_cast<const uint8_t *>(records.constData()),
                               int(classes.entries().size()), sourceClass_, ownerClass_, carried) ||
        lordly_body_equipment(reinterpret_cast<const uint8_t *>(items.constData()), 278, carried,
                              &equipment))
        throw Error("Standalone body equipment exceeds the current tables");
    return {equipment, {}};
}
QVector<ActorFrameLayer> ActorArt::bodyFrame(const Rom &rom, int token, BodyAppearance appearance,
                                             int sepia,
                                             const QMap<int, QByteArray> &artwork) const {
    const auto &currentArtwork = isolated_ ? privateArtwork_ : artwork;
    QVector<ActorFrameLayer> result;
    qsizetype retained = 0;
    for (const auto &selected : bodyLayers(rom, token, appearance)) {
        const auto &layer = selected.metadata;
        const auto sprite = SpriteObject::decode(currentArtwork.contains(layer.art)
                                                     ? currentArtwork.value(layer.art)
                                                     : resource(rom, artKeys_[layer.art], true));
        auto pixels = sprite.frame(selected.child, palettes_.mid(selected.paletteOffset, 512),
                                   sepia, (layer.flags & 0xc0) != 0);
        pixels = pixels.oriented(layer.width, layer.height, layer.flags & 1, layer.flags & 2);
        if ((retained += pixels.image.rgba.size() + pixels.colorAlpha.size()) > 64 * 1024 * 1024)
            throw Error("Body frame exceeds the pixel budget");
        result << ActorFrameLayer{layer, std::move(pixels)};
    }
    return result;
}
} // namespace lordly
