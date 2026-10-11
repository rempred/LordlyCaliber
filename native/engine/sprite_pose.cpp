#include "engine/sprite_pose.h"
#include <bit>
#include <cstring>
namespace lordly {
namespace {
ActorArt binding(const Rom &rom, const QByteArray &record) {
    if (record.size() != 80)
        throw Error("Scene-sprite record must contain 80 bytes");
    return ActorArt::load(rom, std::bit_cast<qint32>(u32(record, 0x30)));
}
} // namespace
SpritePosePlayer::SpritePosePlayer(const Rom &rom, ActorArt art, int state, int variant)
    : rom_(rom), art_(std::move(art)), record_(80, '\0') {
    if (art_.isBody() || state < 0 || state >= art_.poses().size() || variant < 0 || variant > 255)
        throw Error("Invalid scene-sprite pose or variant");
    sprite_.bank = art_.bank();
    sprite_.state = qint16(state);
    sprite_.variant = quint8(variant);
    sprite_.cursor = -1;
    std::memset(sprite_.color, 255, 16);
}
SpritePosePlayer::SpritePosePlayer(const Rom &rom, const QByteArray &record)
    : SpritePosePlayer(rom, binding(rom, record), record) {}
SpritePosePlayer::SpritePosePlayer(const Rom &rom, ActorArt art, const QByteArray &record)
    : rom_(rom), art_(std::move(art)), record_(record) {
    if (record.size() != 80 || art_.isBody() || u32(record, 0x30) != quint32(art_.bank()))
        throw Error("Scene-sprite record does not match its art binding");
    std::memcpy(sprite_.color, record.constData(), 16);
    std::memcpy(sprite_.material, record.constData() + 16, 16);
    sprite_.x = std::bit_cast<qint32>(u32(record, 0x24));
    sprite_.y = std::bit_cast<qint32>(u32(record, 0x28));
    sprite_.bank = std::bit_cast<qint32>(u32(record, 0x30));
    sprite_.cursor = std::bit_cast<qint32>(u32(record, 0x34));
    sprite_.delay = std::bit_cast<qint32>(u32(record, 0x38));
    sprite_.frame = std::bit_cast<qint32>(u32(record, 0x3c));
    sprite_.state = std::bit_cast<qint16>(u16(record, 0x42));
    sprite_.variant = u8(record, 0x4c);
}
int SpritePosePlayer::lookup(void *opaque, const LordlySpritePose *sprite,
                             LordlyPoseRecord *record) {
    auto &self = *static_cast<SpritePosePlayer *>(opaque);
    try {
        if (sprite->bank != self.art_.bank())
            throw Error("Scene sprite requires a different art binding");
        const auto decoded = self.art_.poseRecord(sprite->state, sprite->cursor);
        record->opcode = quint8(decoded.opcode);
        for (int i = 0; i < decoded.operands.size(); ++i)
            record->operands[i] = u8(decoded.operands, i);
        return 1;
    } catch (const Error &e) {
        self.error_ = QString::fromUtf8(e.what());
        return 0;
    }
}
int SpritePosePlayer::sound(void *opaque, const LordlySpritePose *sprite,
                            const LordlyPoseRecord *record) {
    auto &self = *static_cast<SpritePosePlayer *>(opaque);
    try {
        const int key = record->operands[0] * 256 + record->operands[1];
        qsizetype at = 0x212880 + key * 2;
        if (record->opcode == 18 || record->opcode == 20) {
            // The 2D sequencer reads stored Y, not X or a projected coordinate.
            if (key >= 0x24e && key < 0x24e + 0xab)
                at += sprite->y >= 213 ? 0 : sprite->y < 106 ? 4 : 2;
            else
                at = 0x2123a0 + key * 4 + (sprite->y < 160 ? 0 : 2);
        }
        if (at < 0x2123a0 || at + 2 > 0x212f00)
            throw Error("Sprite sound request exceeds its ROM tables");
        if (self.audio_.size() >= 4096)
            throw Error("Sprite sound request budget exceeded");
        self.audio_ << PoseAudioRequest{record->opcode <= 18 ? 0 : 1, u16(self.rom_.bytes(), at)};
        return 1;
    } catch (const Error &e) {
        self.error_ = QString::fromUtf8(e.what());
        return 0;
    }
}
int SpritePosePlayer::advance(int budget) {
    error_.clear();
    if (!invocation_.active)
        audio_.clear();
    const int status =
        lordly_sprite_advance_pose(&sprite_, &invocation_, budget, lookup, sound, this);
    if (status == LORDLY_POSE_BUDGET)
        error_ = "Sprite pose command limit reached; the invocation can resume";
    else if (status == LORDLY_POSE_INVALID)
        error_ = "Sprite pose command contains an invalid field";
    return status;
}
int SpritePosePlayer::query() const {
    if (sprite_.bank != art_.bank())
        throw Error("Scene sprite requires a different art binding");
    return art_.poseRecord(sprite_.state, sprite_.cursor).opcode;
}
QByteArray SpritePosePlayer::record() const {
    auto result = record_;
    std::memcpy(result.data(), sprite_.color, 16);
    std::memcpy(result.data() + 16, sprite_.material, 16);
    put32(result, 0x24, quint32(sprite_.x));
    put32(result, 0x28, quint32(sprite_.y));
    put32(result, 0x30, quint32(sprite_.bank));
    put32(result, 0x34, quint32(sprite_.cursor));
    put32(result, 0x38, quint32(sprite_.delay));
    put32(result, 0x3c, quint32(sprite_.frame));
    put16(result, 0x42, quint16(sprite_.state));
    put8(result, 0x4c, sprite_.variant);
    return result;
}
void SpritePosePlayer::setRotation(qint16 rotation) {
    put16(record_, 0x40, quint16(rotation));
}
} // namespace lordly
