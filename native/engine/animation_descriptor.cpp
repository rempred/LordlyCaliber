#include "engine/animation_descriptor.h"
#include "engine/sprite_object.h"
namespace lordly {
namespace {
QByteArray resource(const Rom &rom, quint32 key, bool compressed) {
    if (!key || key % 2 || key >= 0x4000000 - 0x594280)
        throw Error("Invalid private animation resource key");
    const auto at = key + 0x594280, size = u32(rom.bytes(), at);
    if (size > 16 * 1024 * 1024 || quint64(at) + 4 + size > quint64(rom.bytes().size()))
        throw Error("Private animation resource exceeds its ROM");
    if (!compressed)
        return rom.bytes().mid(at + 4, size);
    if (size < 4 || u32(rom.bytes(), at + 4) > 16 * 1024 * 1024)
        throw Error("Private animation exceeds its decode budget");
    qsizetype consumed = 0;
    const auto decoded =
        decodeLzss(rom.bytes().mid(at + 8, size - 4), u32(rom.bytes(), at + 4), &consumed);
    if (consumed != size - 4)
        throw Error("Private animation has unconsumed resource bytes");
    return decoded;
}
} // namespace
AnimationDescriptor AnimationDescriptor::load(const Rom &rom, quint32 key,
                                              const QMap<int, QByteArray> &artwork) {
    AnimationDescriptor result;
    result.rom_ = rom;
    result.key_ = key;
    const auto bytes = resource(rom, key, false);
    if (bytes.size() < 16 || bytes.size() % 4 || bytes.size() > 65540 * 4)
        throw Error("Invalid private animation descriptor");
    for (int at = 0; at < bytes.size(); at += 4)
        result.members_ << u32(bytes, at);
    for (int i = 0; i < 4; ++i)
        result.controls_[i] = resource(rom, result.members_[i], true);
    const auto &config = result.controls_[2];
    const int count = int(result.members_.size()) - 4;
    if (u32(config, 0) != quint32(count) ||
        quint64(u32(config, 0x1c)) + count > quint64(config.size()) ||
        0x20 + count > config.size() || result.controls_[3].size() % 512)
        throw Error("Private animation configuration does not match its resources");
    result.frameCount();
    PoseBank::parse(result.controls_[1]);
    for (auto it = artwork.cbegin(); it != artwork.cend(); ++it)
        result.setSprite(it.key(), it.value());
    return result;
}
int AnimationDescriptor::frameCount() const {
    const auto first = u32(controls_[0], 0);
    if (!first || first % 4 || first > quint32(controls_[0].size()) || first / 4 > 65536)
        throw Error("Invalid private frame directory");
    return int(first / 4);
}
QByteArray AnimationDescriptor::frame(int token) const {
    if (token < 0 || token >= frameCount())
        throw Error("Pose frame token is outside its metadata directory");
    const auto &bytes = controls_[0];
    const auto at = u32(bytes, token * 4);
    const auto count = u16(bytes, at);
    if (at < quint32(frameCount() * 4) || count > 4096 ||
        quint64(at) + 2 + count * 16 > quint64(bytes.size()))
        throw Error("Private frame exceeds its metadata");
    for (int i = 0; i < count; ++i) {
        const auto p = at + 2 + i * 16;
        const auto art = u16(bytes, p), width = u16(bytes, p + 6), height = u16(bytes, p + 8);
        if (art >= members_.size() - 4 || !width || !height || quint64(width) * height > 4194304)
            throw Error("Private frame has invalid art or dimensions");
    }
    return bytes.mid(at, 2 + count * 16);
}
void AnimationDescriptor::setPoses(const PoseBank &bank) {
    const auto before = poses();
    for (int state = 0; state < bank.programs().size(); ++state) {
        if (state < before.programs().size() &&
            bank.programs()[state].source == before.programs()[state].source)
            continue;
        const auto &program = bank.effective(state);
        bool same = false;
        if (state < before.programs().size()) {
            try {
                same = before.effective(state).source == program.source;
            } catch (const Error &) {
            }
        }
        if (same)
            continue;
        bank.validateEntry(state);
        for (const auto &record : program.records) {
            if (record.opcode == 1)
                frame(u8(record.operands, 0));
            else if (record.opcode == 21)
                frame(u8(record.operands, 0) + 256 * u8(record.operands, 1));
        }
    }
    controls_[1] = bank.encoded();
}
QByteArray AnimationDescriptor::sprite(int art) const {
    if (art < 0 || art >= members_.size() - 4)
        throw Error("Private sprite ordinal is outside its descriptor");
    return artwork_.contains(art) ? artwork_.value(art) : resource(rom_, members_[art + 4], true);
}
void AnimationDescriptor::setSprite(int art, const QByteArray &bytes) {
    if (art < 0 || art >= members_.size() - 4 || bytes.size() > 16 * 1024 * 1024)
        throw Error("Private sprite ordinal or data exceeds its descriptor");
    // Preserve the separate3634 format when untouched; its authoring has a
    // different image contract. Indexed/direct5554 sprites use the shared codec.
    SpriteObject::decode(bytes);
    const auto original = resource(rom_, members_[art + 4], true);
    if (bytes == original)
        artwork_.remove(art);
    else
        artwork_[art] = bytes;
}
QByteArray AnimationDescriptor::identity() const {
    QByteArray bytes;
    for (auto key : members_) {
        QByteArray word(4, 0);
        put32(word, 0, key);
        bytes += word;
    }
    for (const auto &control : controls_) {
        QByteArray size(4, 0);
        put32(size, 0, quint32(control.size()));
        bytes += size + control;
    }
    for (auto it = artwork_.cbegin(); it != artwork_.cend(); ++it) {
        QByteArray header(8, 0);
        put32(header, 0, quint32(it.key()));
        put32(header, 4, quint32(it->size()));
        bytes += header + it.value();
    }
    return sha256(bytes);
}
} // namespace lordly
