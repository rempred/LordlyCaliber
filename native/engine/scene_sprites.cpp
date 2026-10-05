#include "engine/scene_sprites.h"
#include <bit>
namespace lordly {
namespace {
qint32 word(quint32 value) {
    return std::bit_cast<qint32>(value);
}
qint16 half(quint32 value) {
    return std::bit_cast<qint16>(quint16(value));
}
void initializePose(QByteArray &record, const ActorArt &art, int facing) {
    const int state = art.resolve(half(u16(record, 0x44)), facing);
    if (state < 0)
        throw Error("Scene-sprite animation and facing do not select a pose");
    put16(record, 0x42, quint16(state));
    put32(record, 0x34, 0xffffffffU);
    put32(record, 0x38, 0);
    put32(record, 0x3c, 0);
    for (int i = 0; i < 16; ++i) {
        put8(record, i, 255);
        put8(record, i + 16, 0);
    }
}
void advanceImmediate(SpritePosePlayer &sprite) {
    if (sprite.advance(10000) != LORDLY_POSE_OK)
        throw Error(sprite.error());
}
} // namespace
void SceneSprites::checkSlot(int slot) {
    if (slot < 0 || slot >= 30)
        throw Error("Scene-sprite slot is outside 0..29");
}
const SpritePosePlayer *SceneSprites::sprite(int slot) const {
    checkSlot(slot);
    return sprites_[slot] ? &*sprites_[slot] : nullptr;
}
void SceneSprites::supplyRecord(int slot, const QByteArray &record) {
    checkSlot(slot);
    if (record.isEmpty())
        sprites_[slot].reset();
    else
        sprites_[slot] = SpritePosePlayer(rom_, record);
    identities_[slot] = record.isEmpty() ? 0 : ++nextIdentity_;
}
void SceneSprites::supplySprite(int slot, const SpritePosePlayer &sprite) {
    checkSlot(slot);
    sprites_[slot] = sprite;
    identities_[slot] = ++nextIdentity_;
}
void SceneSprites::setPass(int slot, quint8 pass) {
    checkSlot(slot);
    if (sprites_[slot])
        sprites_[slot]->setPass(pass);
}
SpritePosePlayer SceneSprites::create(const QVector<quint32> &args) const {
    const int slot = quint8(args[0]);
    checkSlot(slot);
    if (args[1] == 0xffffffffU || args[2] == 0xffffffffU ||
        (args[4] == 0xffffffffU && args[5] == 0xffffffffU))
        throw Error(
            "A new scene sprite cannot inherit uninitialized source, animation or position");
    const auto art = ActorArt::load(rom_, word(args[1]));
    QByteArray record(80, '\0');
    put32(record, 0x20, 0x8022fdb8); // Native identity only; never called as a host pointer.
    put32(record, 0x24, args[4]);
    put32(record, 0x28, 0U - args[5]);
    put32(record, 0x2c, quint32(slot));
    put32(record, 0x30, args[1]);
    put16(record, 0x40, 180);
    put16(record, 0x44, quint16(args[2]));
    put16(record, 0x48, quint16(args[7]));
    put8(record, 0x4a, quint8(args[6]));
    put8(record, 0x4b, quint8(args[3]));
    put8(record, 0x4c, quint8(args[8]));
    initializePose(record, art, word(args[3]));
    SpritePosePlayer result(rom_, art, record);
    advanceImmediate(result);
    return result;
}
bool SceneSprites::service(quint32 opcode, const QVector<quint32> &args, int &value) {
    int width;
    switch (opcode) {
    case 0x46:
        width = 9;
        break;
    case 0x63:
        width = 10;
        break;
    case 0x64:
        width = 4;
        break;
    case 0x65:
        width = 3; // Comparison mode, comparison target, full-word slot.
        break;
    case 0x66:
        width = 1;
        break;
    default:
        return false;
    }
    if (args.size() != width)
        throw Error("Scene-sprite command has an invalid operand count");
    // Query and release retain full words. Constructors/restart narrow slots.
    const int slot = opcode == 0x65   ? word(args[2])
                     : opcode == 0x66 ? word(args[0])
                                      : quint8(args[0]);
    if (opcode == 0x66 && slot == -1) {
        for (auto &sprite : sprites_)
            sprite.reset();
        identities_ = {};
        return true;
    }
    checkSlot(slot);
    if (opcode == 0x46) {
        sprites_[slot] = create(args);
        identities_[slot] = ++nextIdentity_;
    } else if (opcode == 0x66) {
        sprites_[slot].reset();
        identities_[slot] = 0;
    } else if (opcode == 0x63) {
        // Stage replacement until all required context is available.
        std::optional<SpritePosePlayer> next;
        if (args[9] == 0xffffffffU)
            next = create({args[0], args[1], args[2], 0, args[4], args[5], 1, 100, args[3]});
        else {
            if (!sprites_[slot])
                throw Error("Scene-sprite reuse requires an existing sprite");
            next = sprites_[slot];
        }
        qint16 rotation;
        if (args[8] == 0xffffffffU)
            rotation = half(args[6]);
        else {
            if (!rotation_)
                throw Error("Scene-sprite rotation requires its current path state");
            rotation = rotation_(word(args[7]), word(args[8]));
        }
        next->setRotation(rotation);
        sprites_[slot] = std::move(next);
        if (args[9] == 0xffffffffU)
            identities_[slot] = ++nextIdentity_;
    } else {
        if (!sprites_[slot])
            throw Error("Scene-sprite command requires an existing sprite");
        if (opcode == 0x65) {
            value = sprites_[slot]->query();
            return true;
        }
        auto record = sprites_[slot]->record();
        put32(record, 0x2c, quint32(slot));
        if (args[1] != 0xffffffffU)
            put32(record, 0x30, args[1]);
        // The parser passes the full word; only exactly FFFFFFFF inherits.
        if (args[2] != 0xffffffffU)
            put16(record, 0x44, quint16(args[2]));
        put8(record, 0x4c, quint8(args[3]));
        const auto art = ActorArt::load(rom_, word(u32(record, 0x30)));
        initializePose(record, art, 0);
        SpritePosePlayer next(rom_, art, record);
        advanceImmediate(next);
        sprites_[slot] = std::move(next);
    }
    return true;
}
bool SceneSprites::advancePoses(int instructionBudget) {
    if (instructionBudget < 1)
        throw Error("Sprite pose budget must be positive");
    for (; nextPoseSlot_ < 30; ++nextPoseSlot_) {
        auto &sprite = sprites_[nextPoseSlot_];
        if (!sprite)
            continue;
        const int status = sprite->advance(instructionBudget);
        if (status == LORDLY_POSE_BUDGET)
            return false;
        if (status != LORDLY_POSE_OK)
            throw Error(sprite->error());
    }
    nextPoseSlot_ = 0;
    return true;
}
QVector<SceneSpriteDraw> SceneSprites::draws(qint32 renderPass, quint8 special) const {
    const int pass = half(quint32(renderPass));
    QVector<SceneSpriteDraw> result;
    for (int slot = 0; slot < 30; ++slot) {
        if (!sprites_[slot])
            continue;
        const auto record = sprites_[slot]->record();
        if (u8(record, 0x4a) != pass)
            continue;
        const auto animation = u16(record, 0x44);
        const bool specialAnimation = animation == 800 || animation == 801;
        if (specialAnimation != (special != 0))
            continue;
        const int rotation = half(u16(record, 0x40));
        const int scale = half(u16(record, 0x48));
        const auto &pose = sprites_[slot]->sprite();
        result << SceneSpriteDraw{slot,
                                  quint8(pass),
                                  float(pose.x),
                                  float(pose.y),
                                  rotation == 180 ? 0.f : float(rotation + 180),
                                  float(scale) / 100.f};
    }
    return result;
}
std::array<quint8, 4> SceneSprites::primitiveColor(int slot, int layer, quint8 alpha,
                                                   const std::array<qint16, 3> &globalRgb,
                                                   const QByteArray &colorTransition) const {
    const auto current = sprite(slot);
    if (!current || layer < 0 || layer >= 16)
        throw Error("Sprite layer has no native color byte");
    if (!colorTransition.isEmpty() && colorTransition.size() != 80)
        throw Error("Scene color transition must contain 80 bytes");
    std::array<quint8, 4> result;
    for (int i = 0; i < 3; ++i)
        // The original reads the transition's first current-color byte for
        // all three channels, multiplying the separate global signed RGB.
        result[i] = colorTransition.isEmpty()
                        ? quint8(globalRgb[i])
                        : quint8(int(u8(colorTransition, 4)) * globalRgb[i] / 255);
    result[3] = quint8(int(alpha) * current->sprite().color[layer] / 255);
    return result;
}
} // namespace lordly
