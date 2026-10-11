#include "engine/scene_transforms.h"
#include "game/director/scene_matrix.h"
#include "game/director/scene_transform.h"
#include <bit>
#include <cmath>
#include <cstring>
namespace lordly {
namespace {
QByteArray resource(const Rom &rom, quint32 key) {
    const quint64 at = quint64(key) + 0x594280;
    if (!key || key >= 0x10000000 || key % 2 || at + 4 > quint64(rom.bytes().size()))
        throw Error("Scene transform resource key is outside the ROM");
    const auto size = u32(rom.bytes(), at);
    if (!size || size > 4 * 1024 * 1024 || at + 4 + size > quint64(rom.bytes().size()))
        throw Error("Scene transform resource exceeds its envelope");
    return rom.bytes().mid(at + 4, size);
}
void checkChannel(int index) {
    if (index < 0 || index >= 20)
        throw Error("Scene transform channel is outside 0..19");
}
} // namespace
int SceneTransformAsset::selectorCount(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Scene transforms require US revision zero");
    const auto table = resource(rom, 0x19a63ec);
    if (table.size() % 4)
        throw Error("Scene transform directory is truncated");
    return int(table.size() / 4);
}
SceneTransformAsset SceneTransformAsset::load(const Rom &rom, int selector) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Scene transforms require US revision zero");
    const auto table = resource(rom, 0x19a63ec);
    if (table.size() % 4 || selector < 0 || selector >= table.size() / 4)
        throw Error("Scene transform selector is outside its current ROM directory");
    const auto key = u32(table, selector * 4);
    const auto packed = resource(rom, key);
    const auto size = u32(packed, 0);
    if (size < 8 || size > 4 * 1024 * 1024)
        throw Error("Scene transform decoded size is invalid");
    qsizetype consumed = 0;
    const auto decoded = decodeLzss(packed.mid(4), size, &consumed);
    if (consumed != packed.size() - 4)
        throw Error("Scene transform has unrecognized compressed trailing bytes");
    return parse(decoded, key);
}
SceneTransformAsset SceneTransformAsset::parse(QByteArray bytes, quint32 key) {
    const auto frames = u32(bytes, 0), count = u32(bytes, 4);
    if (!frames || frames > 65536 || count > 65536 ||
        quint64(frames) * 480 + 8 > quint64(bytes.size()))
        throw Error("Invalid scene transform resource header");
    SceneTransformAsset result{key, std::move(bytes), int(frames), {}};
    qsizetype cursor = 8 + qsizetype(frames) * 480;
    for (quint32 group = 0; group < count; ++group) {
        QVector<int> sequence;
        for (;;) {
            const auto frame = u32(result.decoded, cursor),
                       continuation = u32(result.decoded, cursor + 4);
            cursor += 8;
            // Retail resource 33 contains three unused groups whose IDs exceed
            // its frame count. Preserve them; validate the selected group at start.
            sequence << qint32(frame);
            if (continuation == 0xffffffffU)
                break;
        }
        result.groups << sequence;
    }
    if (cursor != result.decoded.size())
        throw Error("Scene transform resource has unowned decoded bytes");
    return result;
}
SceneTransforms::SceneTransforms() {
    lordly_transform_identity(channels_);
}
QByteArray SceneTransforms::channel(int index) const {
    checkChannel(index);
    return {reinterpret_cast<const char *>(channels_[index]), 88};
}
void SceneTransforms::supplyChannel(int index, const QByteArray &record) {
    checkChannel(index);
    if (record.size() != 88)
        throw Error("Scene transform record must contain 88 bytes");
    for (int at = 64; at < 88; at += 4)
        if (!std::isfinite(std::bit_cast<float>(u32(record, at))))
            throw Error("Scene transform fields must be finite");
    std::memcpy(channels_[index], record.constData(), 88);
}
void SceneTransforms::start(const Rom &rom, int selector, int group, qint32 duration) {
    start(SceneTransformAsset::load(rom, selector), group, duration);
}
void SceneTransforms::start(const SceneTransformAsset &asset, int group, qint32 duration) {
    if (group < 0 || group >= asset.groups.size() || asset.groups[group].isEmpty() ||
        asset.groups[group].size() > 20)
        throw Error("Scene transform group must contain one through twenty keyframes");
    for (const auto frame : asset.groups[group])
        if (frame < 0 || frame >= asset.frameCount)
            throw Error("Transform sequence references an absent keyframe");
    auto next = *this;
    next.job_ = Job{asset, asset.groups[group], 0, duration};
    next.prepare();
    *this = std::move(next);
}
void SceneTransforms::prepare() {
    auto &job = *job_;
    const auto raw = [&](int ordinal) -> const uint8_t * {
        const auto frame = job.sequence[ordinal];
        const quint64 end = 8 + (quint64(frame) + 1) * 480;
        if (frame < 0 || end > quint64(job.asset.decoded.size()))
            throw Error("Transform keyframe exceeds its resource");
        return reinterpret_cast<const uint8_t *>(job.asset.decoded.constData() + 8 + frame * 480);
    };
    const bool last = job.next == job.sequence.size() - 1;
    if (!lordly_transform_prepare(channels_, job.delta, raw(job.next),
                                  last ? nullptr : raw(job.next + 1), job.duration))
        throw Error("Scene transform segment has invalid integer division");
    if (last)
        job_.reset();
    else {
        job.remaining = quint32(job.duration);
        ++job.next;
    }
}
void SceneTransforms::advance() {
    if (!job_)
        return;
    auto next = *this;
    if (!lordly_transform_advance(next.channels_, next.job_->delta))
        throw Error("Scene transform update produced nonfinite fields");
    if (--next.job_->remaining == 0)
        next.prepare();
    *this = std::move(next);
}
void SceneTransforms::prepareMatrices(const Rom &rom, const SceneCamera &camera, int count) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Scene transform matrix constants require US revision zero");
    float values[14];
    double constants[4];
    for (int i = 0; i < 14; ++i)
        values[i] = std::bit_cast<float>(u32(camera.bank(), i * 4));
    // Mode-zero image maps ROM 2B8A60..2B8A78 to RAM 80243C60..80243C78.
    for (int i = 0; i < 3; ++i) {
        const auto at = 0x2b8a60 + i * 8;
        constants[i] =
            std::bit_cast<double>(quint64(u32(rom.bytes(), at)) << 32 | u32(rom.bytes(), at + 4));
    }
    // Saved-image equality in the shared-Actor projection reference qualifies
    // this resident constant separately from the mode-zero overlay mapping.
    constants[3] = std::bit_cast<float>(u32(rom.bytes(), 0x3ab50));
    if (!lordly_channel_matrices(channels_, count, values, constants))
        throw Error("Scene transform matrix exceeds its channel or fixed-point bounds");
}
bool SceneTransforms::done() const {
    return lordly_transform_done(job_.has_value());
}
QByteArray SceneTransforms::layerMatrix(int index) const {
    checkChannel(index);
    QByteArray result(64, 0);
    if (!lordly_channel_layer_matrix(reinterpret_cast<uint8_t *>(result.data()), channels_[index]))
        throw Error("Scene layer matrix exceeds fixed-point bounds");
    return result;
}
quint32 SceneTransforms::remaining() const {
    return job_ ? job_->remaining : 0;
}
int SceneTransforms::nextKeyframe() const {
    return job_ ? job_->next : -1;
}
QByteArray SceneTransforms::deltas() const {
    if (!job_)
        return {};
    QByteArray result(480, 0);
    for (int channel = 0; channel < 20; ++channel)
        for (int field = 0; field < 6; ++field)
            put32(result, (channel * 6 + field) * 4,
                  std::bit_cast<quint32>(job_->delta[channel][field]));
    return result;
}
} // namespace lordly
