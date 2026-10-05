#include "engine/scene_image_motion.h"
#include <bit>
#include <cmath>
namespace lordly {
namespace {
void validate(const QByteArray &context, const QByteArray &channel) {
    if (context.size() != 0x848 || channel.size() != 88)
        throw Error("Image motion requires its presentation context and channel-one transform");
}
float real(const QByteArray &bytes, int at) {
    return std::bit_cast<float>(u32(bytes, at));
}
void real(QByteArray &bytes, int at, float value) {
    put32(bytes, at, std::bit_cast<quint32>(value));
}
LordlyImageZoom zoom(const QByteArray &context) {
    return {u8(context, 0x83c), u32(context, 0x828), u32(context, 0x830)};
}
void zoom(QByteArray &context, const LordlyImageZoom &value) {
    put8(context, 0x83c, value.state);
    put32(context, 0x828, value.elapsed);
    put32(context, 0x830, value.duration);
}
} // namespace
SceneImageMotion::SceneImageMotion(const Rom &rom) {
    // Entire RAM80243C98..80243CE8 uniquely matches ROM2B8A98..2B8AE8 in
    // retained opening RAM. These data locations are not inferred from code.
    for (int i = 0; i < 4; ++i) {
        const int at = 0x2b8ac8 + i * 8;
        constants_[i] =
            std::bit_cast<double>((quint64(u32(rom.bytes(), at)) << 32) | u32(rom.bytes(), at + 4));
        if (!std::isfinite(constants_[i]))
            throw Error("Image zoom has a nonfinite ROM constant");
    }
}
void SceneImageMotion::start(const QVector<quint32> &operands, QByteArray &context,
                             const QByteArray &channel) {
    validate(context, channel);
    if (operands.size() != 7)
        throw Error("Scripted image motion requires seven operands");
    int32_t args[7];
    for (int i = 0; i < 7; ++i)
        args[i] = std::bit_cast<qint32>(operands[i]);
    LordlyImageMotion motion;
    auto z = zoom(context);
    if (!lordly_image_motion_start(&motion, &z, args, real(channel, 0x40), real(channel, 0x44)))
        throw Error("Scripted image motion produces a nonfinite rate");
    auto next = context;
    zoom(next, z);
    context = std::move(next);
    motion_ = motion;
}
void SceneImageMotion::advance(QByteArray &context, QByteArray &channel) {
    if (!motion_)
        return;
    validate(context, channel);
    auto motion = *motion_;
    auto z = zoom(context);
    auto x = real(channel, 0x40), y = real(channel, 0x44), scale = real(channel, 0x54);
    const int present = lordly_image_motion_advance(&motion, &z, &x, &y, &scale, constants_.data());
    if (present < 0)
        throw Error("Scripted image update produces nonfinite coordinates or zoom");
    auto nextContext = context, nextChannel = channel;
    zoom(nextContext, z);
    real(nextChannel, 0x40, x);
    real(nextChannel, 0x44, y);
    real(nextChannel, 0x54, scale);
    context = std::move(nextContext);
    channel = std::move(nextChannel);
    if (present)
        motion_ = motion;
    else
        motion_.reset();
}
QByteArray SceneImageMotion::record() const {
    if (!motion_)
        return {};
    QByteArray out(16, 0);
    real(out, 0, motion_->step_x);
    real(out, 4, motion_->step_y);
    put32(out, 8, quint32(motion_->duration));
    put32(out, 12, motion_->elapsed);
    return out;
}
void SceneImageMotion::supplyRecord(QByteArray record) {
    if (record.isEmpty()) {
        motion_.reset();
        return;
    }
    if (record.size() != 16 || !std::isfinite(real(record, 0)) || !std::isfinite(real(record, 4)))
        throw Error("Image motion needs a complete finite job record");
    motion_ = LordlyImageMotion{real(record, 0), real(record, 4),
                                std::bit_cast<qint32>(u32(record, 8)), u32(record, 12)};
}
} // namespace lordly
