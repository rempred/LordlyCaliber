#include "engine/direct_pose.h"
namespace lordly {
namespace {
QByteArray initialized(quint16 handle, quint16 selector) {
    QByteArray bytes(80, 0);
    put32(bytes, 0, handle);
    if (lordly_direct_initialize(reinterpret_cast<uint8_t *>(bytes.data()), selector, nullptr,
                                 nullptr))
        throw Error("Direct pose initialization failed");
    return bytes;
}
struct Invocation {
    DirectPosePlayer *player;
    const std::function<quint32()> *random;
};
} // namespace
DirectPosePlayer::DirectPosePlayer(const Rom &rom, QByteArray record)
    : rom_(rom), record_(std::move(record)) {
    if (record_.size() != 80 || u32(record_, 0) > 65535)
        throw Error("Invalid direct descriptor pose record");
    art_ = ActorArt::loadDirect(rom_, quint16(u32(record_, 0)));
}
DirectPosePlayer::DirectPosePlayer(const Rom &rom, quint16 handle, quint16 selector)
    : DirectPosePlayer(rom, initialized(handle, selector)) {}
int DirectPosePlayer::lookup(void *context, const uint8_t *bytes, LordlyPoseRecord *out) {
    auto &self = *static_cast<Invocation *>(context)->player;
    try {
        const QByteArray record(reinterpret_cast<const char *>(bytes), 80);
        const auto command = self.art_.poseRecord(qint16(u16(record, 4)), qint16(u16(record, 6)));
        out->opcode = quint8(command.opcode);
        for (int i = 0; i < command.operands.size(); ++i)
            out->operands[i] = u8(command.operands, i);
        return 1;
    } catch (const Error &e) {
        self.error_ = QString::fromUtf8(e.what());
        return 0;
    }
}
int DirectPosePlayer::sound(void *context, const uint8_t *bytes, const LordlyPoseRecord *command) {
    auto &self = *static_cast<Invocation *>(context)->player;
    if (self.sounds_.size() >= 4096) {
        self.error_ = "Direct pose audio request budget exceeded";
        return 0;
    }
    const QByteArray record(reinterpret_cast<const char *>(bytes), 80);
    self.sounds_ << DirectPoseSound{command->opcode,
                                    quint16(command->operands[0] * 256 + command->operands[1]),
                                    u32(record, 0x4c)};
    return 1;
}
int DirectPosePlayer::advance(int budget, const std::function<quint32()> &random) {
    error_.clear();
    if (!invocation_.active)
        sounds_.clear();
    Invocation context{this, &random};
    LordlyDirectRandom rng =
        random ? +[](void *p) -> uint32_t { return (*static_cast<Invocation *>(p)->random)(); }
               : nullptr;
    const int result = lordly_direct_advance(reinterpret_cast<uint8_t *>(record_.data()),
                                             &invocation_, budget, lookup, sound, rng, &context);
    if (result == LORDLY_POSE_BUDGET)
        error_ = "Direct pose command budget reached";
    else if (result == LORDLY_POSE_SHARED && error_.isEmpty())
        error_ = "Direct pose needs a random source";
    else if (result == LORDLY_POSE_INVALID)
        error_ = "Direct pose exceeded its native stack or field bounds";
    return result;
}
} // namespace lordly
