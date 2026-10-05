#include "engine/stage_orthographic.h"
#include "game/director/scene_matrix.h"
#include <cmath>
#include <memory>
namespace lordly {
StageOrthographic StageOrthographic::load(const Rom &rom, const StagePlacements &placements,
                                          const std::function<quint32()> &random) {
    StageOrthographic result;
    for (const auto &source : placements.orthographic()) {
        const auto resolved = source.resolve();
        const auto handle = u16(resolved, 0), variant = u16(resolved, 2);
        const int count = u16(resolved, 14);
        if (count > 4096 - result.props_.size())
            throw Error("Stage prop pool exceeds its record budget");
        for (int i = 0; i < count; ++i) {
            quint16 selector = u16(resolved, 4);
            QByteArray header(20, 0);
            put16(header, 6, variant);
            put16(header, 4, u16(resolved, 10));
            if (source.randomized()) {
                const int width = qint16(u16(resolved, 20)) - qint16(u16(resolved, 16));
                const int height = qint16(u16(resolved, 22)) - qint16(u16(resolved, 18));
                // The random pose upper bound is resolver+0C; +0A is Z.
                const int poses = u16(resolved, 12) - selector + 1;
                if (!random || poses <= 0 || width <= 0 || height <= 0)
                    throw Error(
                        "Random Stage prop requires valid ranges and an explicit random source");
                selector = quint16(selector + (random() & 0x7fffffffU) % quint32(poses));
                for (int j = 0; j < 4; ++j)
                    put16(header, 8 + j * 2, u16(resolved, 16 + j * 2));
                put16(
                    header, 0,
                    quint16(qint16(u16(resolved, 16)) + (random() & 0x7fffffffU) % quint32(width)));
                put16(header, 2,
                      quint16(qint16(u16(resolved, 18)) +
                              (random() & 0x7fffffffU) % quint32(height)));
                put16(header, 16, 1);
            } else {
                put16(header, 0, u16(resolved, 6));
                put16(header, 2, u16(resolved, 8));
            }
            DirectPosePlayer pose(rom, handle, selector);
            result.props_ << StageFlatProp{header, std::move(pose)};
        }
    }
    return result;
}
StageOrthographic StageOrthographic::fromRecords(const Rom &rom,
                                                 const QVector<QByteArray> &records) {
    if (records.size() > 4096)
        throw Error("Stage prop pool exceeds its record budget");
    StageOrthographic result;
    for (const auto &record : records) {
        if (record.size() != 100)
            throw Error("Stage prop record requires100 bytes");
        result.props_ << StageFlatProp{record.left(20), DirectPosePlayer(rom, record.mid(20))};
    }
    return result;
}
void StageOrthographic::advance() {
    auto next = props_;
    for (auto &prop : next) {
        if (prop.pose.advance(10000) != LORDLY_POSE_OK)
            throw Error(prop.pose.error());
        if (!(u16(prop.header, 16) & 1))
            continue;
        int offsets[3];
        for (int i = 0; i < 3; ++i)
            offsets[i] = qint16(u16(prop.pose.record(), 12 + i * 2));
        for (int axis = 0; axis < 2; ++axis) {
            const int origin = qint16(u16(prop.header, axis * 2));
            const int low = qint16(u16(prop.header, 8 + axis * 2));
            const int high = qint16(u16(prop.header, 12 + axis * 2));
            if (origin + offsets[axis] < low)
                offsets[axis] = qint16(high + offsets[axis] - low);
            if (origin + offsets[axis] > high)
                offsets[axis] = qint16(low + offsets[axis] - high);
        }
        prop.pose.setOffsets(qint16(offsets[0]), qint16(offsets[1]), qint16(offsets[2]));
    }
    props_ = std::move(next);
}
int StageOrthographic::drawPass(const QByteArray &record, float backgroundZ, float foregroundZ) {
    if (record.size() != 100 || !std::isfinite(backgroundZ) || !std::isfinite(foregroundZ))
        throw Error("Invalid Stage prop draw context");
    const float z = qint16(u16(record, 4));
    if (std::abs(backgroundZ - z) <= std::abs(foregroundZ - z))
        return 0;
    return z <= foregroundZ ? 2 : 1;
}
QByteArray StageOrthographic::modelMatrix(const QByteArray &record, const QByteArray &parent) {
    if (record.size() != 100 || parent.size() != 64)
        throw Error("Invalid Stage prop matrix input");
    float translation[16], base[16], combined[16];
    uint8_t packed[64];
    lordly_matrix_translate(translation, float(qint16(u16(record, 0)) + qint16(u16(record, 0x20))),
                            float(qint16(u16(record, 2)) + qint16(u16(record, 0x22))),
                            float(qint16(u16(record, 4)) + qint16(u16(record, 0x24))));
    if (!lordly_matrix_pack(packed, translation))
        throw Error("Stage translation exceeds fixed-point range");
    lordly_matrix_unpack(translation, packed);
    lordly_matrix_unpack(base, reinterpret_cast<const uint8_t *>(parent.constData()));
    lordly_matrix_multiply(combined, translation, base);
    if (!lordly_matrix_pack(packed, combined))
        throw Error("Stage model exceeds fixed-point range");
    return QByteArray(reinterpret_cast<const char *>(packed), 64);
}
QVector<ActorRasterLayer>
StageFlatRenderer::project(const StageOrthographic &stage, int pass, const SceneCamera &camera,
                           const QByteArray &parent, int foregroundSelector,
                           const std::array<qint16, 3> &globalRgb,
                           std::optional<RenderColor> transition, bool warmTint,
                           RenderColor environment, float backgroundZ, float foregroundZ, bool sepia) {
    if (pass < 0 || pass > 2)
        throw Error("Stage prop pass must be in 0..2");
    QVector<ActorRasterLayer> output;
    int index = 0;
    for (const auto &prop : stage.props()) {
        const int slot = index++;
        const auto record = prop.record();
        if (StageOrthographic::drawPass(record, backgroundZ, foregroundZ) != pass)
            continue;
        const auto &pose = prop.pose.record();
        const int frame = qint16(u16(pose, 10)), variant = u16(prop.header, 6);
        const auto key =
            QString("%1/%2/%3/%4").arg(prop.pose.art().descriptorKey()).arg(frame).arg(variant).arg(sepia);
        auto frames = frames_.object(key);
        if (!frames) {
            auto decoded = std::make_unique<QVector<ActorFrameLayer>>(
                prop.pose.art().frame(rom_, frame, variant, sepia));
            qsizetype bytes = 0;
            for (const auto &layer : *decoded)
                bytes += layer.pixels.image.rgba.size() + layer.pixels.colorAlpha.size() + 128;
            const int cost = int(qMax<qsizetype>(1, (bytes + 1023) / 1024));
            if (cost > frames_.maxCost())
                throw Error("Stage frame exceeds rendering cache budget");
            frames = decoded.get();
            frames_.insert(key, decoded.release(), cost);
        }
        // Local raster adapter only; the Stage prop remains its native100-byte record.
        QByteArray adapter(336, 0);
        put32(adapter, 0xe4, quint32(slot));
        const auto model = StageOrthographic::modelMatrix(record, parent);
        adapter.replace(0xa0, 64, model);
        int alpha = transition ? (*transition)[3] : 255;
        auto rgb = globalRgb;
        for (int c = 0; c < 3; ++c)
            if (transition)
                rgb[c] = qint16(rgb[c] * (*transition)[c] / 255);
        auto material = ActorRenderState::main(environment);
        for (int i = 0; i < frames->size(); ++i) {
            if (i >= 16)
                throw Error("Stage frame exceeds its16 opacity fields");
            const auto &layer = (*frames)[i];
            if (layer.pixels.sourceColorFormat == 4) {
                if (foregroundSelector == 47 || foregroundSelector == 48)
                    rgb.fill(0);
                if (warmTint) {
                    rgb = {255, 200, 130};
                    if (transition)
                        for (int c = 0; c < 3; ++c)
                            rgb[c] = qint16(rgb[c] * (*transition)[c] / 255);
                    if (transition)
                        alpha = alpha * (*transition)[3] / 255;
                }
            }
            alpha = alpha * u8(pose, 0x12 + i) / 255;
            put8(adapter, i, quint8(alpha));
            for (int c = 0; c < 3; ++c)
                put8(adapter, 0x142 + c, quint8(rgb[c]));
            QByteArray commands(8, 0);
            put32(commands, 0, layer.pixels.dualTexture ? 0xfc11a7ff : 0xfc1197ff);
            put32(commands, 4, 0xfffffe38);
            material.apply(commands);
            output << ActorRenderer::projectLayer(layer, adapter, i, false, camera, {255, 255, 255},
                                                  1, 0, 255, &material, LayerBinding::StageFlat);
        }
    }
    return output;
}
} // namespace lordly
