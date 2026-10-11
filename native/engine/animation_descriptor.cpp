#include "engine/animation_descriptor.h"
#include "engine/sprite_object.h"
#include <QJsonArray>
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
    result.originalArtCount_ = count;
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
int AnimationDescriptor::appendFrame(const QByteArray &row) {
    const int count = frameCount();
    if (count >= 65536 || row.size() < 2 || row.size() != 2 + u16(row, 0) * 16 ||
        controls_[0].size() + 4 + row.size() > 16 * 1024 * 1024)
        throw Error("New animation frame exceeds its metadata budget");
    QByteArray bytes((count + 1) * 4, 0);
    for (int token = 0; token < count; ++token)
        put32(bytes, token * 4, u32(controls_[0], token * 4) + 4);
    put32(bytes, count * 4, quint32(controls_[0].size() + 4));
    bytes += controls_[0].mid(count * 4) + row;
    auto next = *this;
    next.controls_[0] = bytes;
    next.frame(count);
    *this = std::move(next);
    return count;
}
void AnimationDescriptor::setMetadata(const QByteArray &bytes) {
    if (bytes == controls_[0])
        return;
    if (bytes.size() > 16 * 1024 * 1024)
        throw Error("Private metadata exceeds its data budget");
    auto next = *this;
    next.controls_[0] = bytes;
    const int oldCount = frameCount(), count = next.frameCount();
    if (count < oldCount)
        throw Error("Private metadata must retain original frames");
    const int shift = (count - oldCount) * 4;
    for (int token = 0; token < oldCount; ++token)
        if (u32(bytes, token * 4) != u32(controls_[0], token * 4) + shift)
            throw Error("Private metadata changed an original frame pointer");
    const auto body = controls_[0].mid(oldCount * 4);
    if (bytes.mid(count * 4, body.size()) != body)
        throw Error("Private metadata changed original frame data");
    qsizetype cursor = count * 4 + body.size();
    for (int token = oldCount; token < count; ++token) {
        if (u32(bytes, token * 4) != cursor)
            throw Error("Private metadata has overlapping or unclaimed new frames");
        cursor += next.frame(token).size();
    }
    if (cursor != bytes.size())
        throw Error("Private metadata has an unclaimed tail");
    controls_[0] = bytes;
}
void AnimationDescriptor::setPoses(const PoseBank &bank) {
    const auto before = poses();
    if (bank.programs().size() < before.programs().size())
        throw Error("Private animations must retain existing pose selectors");
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
    const auto key = members_[art + 4];
    const auto original = key ? resource(rom_, key, true) : QByteArray{};
    if (key && bytes == original)
        artwork_.remove(art);
    else
        artwork_[art] = bytes;
}
int AnimationDescriptor::appendSprite(const QByteArray &bytes, int policy,
                                      const QByteArray &palette) {
    const int count = int(members_.size()) - 4;
    const auto &config = controls_[2];
    const auto map = u32(config, 0x1c);
    if (count >= 65536 || policy < 0 || policy > 2 || palette.size() != 512 ||
        bytes.size() > 16 * 1024 * 1024 || map < quint32(32 + count) ||
        quint64(map) + count > quint64(config.size()) || config.size() + 2 > 16 * 1024 * 1024)
        throw Error("New sprite exceeds its descriptor or has an incompatible configuration");
    SpriteObject::decode(bytes);
    auto next = *this;
    auto &palettes = next.controls_[3];
    int bank = -1;
    for (int i = 0; i < palettes.size() / 512 && i < 256; ++i)
        if (palettes.mid(i * 512, 512) == palette) {
            bank = i;
            break;
        }
    if (bank < 0) {
        bank = int(palettes.size() / 512);
        if (bank >= 256)
            throw Error("A private sprite descriptor supports at most 256 palette banks");
        palettes += palette;
    }
    // Both arrays contain bytes. Insert only their new entries; retain padding
    // and unknown configuration data, moving the palette-map offset with it.
    auto &updated = next.controls_[2];
    updated.insert(32 + count, char(policy));
    updated.insert(map + 1 + count, char(bank));
    put32(updated, 0, quint32(count + 1));
    put32(updated, 0x1c, map + 1);
    next.members_ << 0;
    next.artwork_[count] = bytes;
    *this = std::move(next);
    return count;
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
QJsonObject AnimationDescriptor::json() const {
    QJsonObject artwork;
    for (auto it = artwork_.cbegin(); it != artwork_.cend(); ++it)
        if (it.key() < originalArtCount_)
            artwork[QString::number(it.key())] = QString::fromLatin1(it->toBase64());
    QJsonObject result{{"key", double(key_)},
                       {"metadata", QString::fromLatin1(controls_[0].toBase64())},
                       {"poses", QString::fromLatin1(controls_[1].toBase64())},
                       {"artwork", artwork}};
    QJsonArray extras;
    for (int art = originalArtCount_; art < members_.size() - 4; ++art)
        extras << QJsonObject{
            {"data", QString::fromLatin1(sprite(art).toBase64())},
            {"policy", u8(controls_[2], 32 + art)},
            {"palette",
             QString::fromLatin1(controls_[3]
                                     .mid(u8(controls_[2], u32(controls_[2], 28) + art) * 512, 512)
                                     .toBase64())}};
    if (!extras.isEmpty())
        result["extraSprites"] = extras;
    return result;
}
AnimationDescriptor AnimationDescriptor::fromJson(const Rom &rom, const QJsonObject &object) {
    for (const auto &key : object.keys())
        if (!QStringList{"key", "metadata", "poses", "artwork", "extraSprites"}.contains(key))
            throw Error("Unknown saved private animation field");
    if (!object["poses"].isString() || !object["artwork"].isObject() ||
        (object.contains("extraSprites") && !object["extraSprites"].isArray()))
        throw Error("Invalid saved private animation descriptor");
    const auto key = object["key"].toInteger(-1);
    if (key <= 0 || key >= 0x4000000 - 0x594280 || object["key"].toDouble() != double(key))
        throw Error("Invalid saved private descriptor identity");
    auto result = load(rom, quint32(key));
    const auto decode = [](const QJsonValue &value) {
        if (!value.isString() || value.toString().size() > 24 * 1024 * 1024)
            throw Error("Private animation exceeds its saved data budget");
        const auto text = value.toString().toLatin1();
        const auto bytes = QByteArray::fromBase64(text, QByteArray::AbortOnBase64DecodingErrors);
        if (bytes.toBase64() != text)
            throw Error("Invalid private animation encoding");
        return bytes;
    };
    quint64 addedBytes = 0;
    for (const auto &value : object["extraSprites"].toArray()) {
        const auto extra = value.toObject();
        const auto policy = extra["policy"].toInteger(-1);
        if (extra.size() != 3 || policy < 0 || policy > 2 ||
            extra["policy"].toDouble() != double(policy))
            throw Error("Invalid saved private sprite binding");
        const auto bytes = decode(extra["data"]);
        addedBytes += bytes.size();
        if (addedBytes > 512ULL * 1024 * 1024)
            throw Error("Private sprites exceed their saved data budget");
        result.appendSprite(bytes, int(policy), decode(extra["palette"]));
    }
    if (object.contains("metadata"))
        result.setMetadata(decode(object["metadata"]));
    result.setPoses(PoseBank::parse(decode(object["poses"])));
    const auto edits = object["artwork"].toObject();
    for (auto it = edits.begin(); it != edits.end(); ++it) {
        bool ok = false;
        const int art = it.key().toInt(&ok);
        if (!ok || QString::number(art) != it.key() || art >= result.originalArtCount_)
            throw Error("Invalid private sprite ordinal");
        result.setSprite(art, decode(it.value()));
    }
    return result;
}
} // namespace lordly
