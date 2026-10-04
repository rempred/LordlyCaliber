#include "engine/actor_instance.h"
#include <bit>
#include <cmath>
#include <cstring>
namespace lordly {
namespace {
LordlyActorRecord raw(const QByteArray &record) {
    if (record.size() != LORDLY_ACTOR_RECORD_BYTES)
        throw Error("Actor record must contain 336 bytes");
    LordlyActorRecord result;
    std::memcpy(result.bytes, record.constData(), sizeof(result.bytes));
    return result;
}
QByteArray bytes(const LordlyActorRecord &record) {
    return {reinterpret_cast<const char *>(record.bytes), sizeof(record.bytes)};
}
struct Context {
    Rom rom;
    ActorInstance::Height height;
    QMap<int, ActorArt> art;
    QString error;
};
int resolve(void *context, int32_t bank, int32_t animation, int32_t facing, int *state) {
    auto &c = *static_cast<Context *>(context);
    try {
        if (!c.art.contains(bank))
            c.art.insert(bank, ActorArt::load(c.rom, bank));
        *state = c.art[bank].resolve(animation, facing);
        return 1;
    } catch (const Error &e) {
        c.error = QString::fromUtf8(e.what());
        return 0;
    }
}
int height(void *context, int mode, float x, float z, float *result) {
    auto &c = *static_cast<Context *>(context);
    try {
        if (!c.height) {
            c.error = "Actor placement requires the scene terrain height";
            return 0;
        }
        *result = c.height(mode, x, z);
        return 1;
    } catch (const std::exception &e) {
        c.error = QString::fromUtf8(e.what());
        return 0;
    }
}
void require(int result, const Context &context) {
    if (result != LORDLY_ACTOR_OK)
        throw Error(context.error.isEmpty() ? "Invalid actor construction or State request"
                                            : context.error);
}
ActorPosePlayer player(const Rom &rom, const QByteArray &record) {
    const auto r = raw(record);
    LordlyActorPose fields{};
    lordly_actor_read_pose(&r, &fields);
    auto art = fields.decoder_mode ? ActorArt::loadBody(rom, fields.bank, fields.owner,
                                                        fields.variant, fields.flag_b)
                                   : ActorArt::load(rom, fields.bank);
    // Special body classes clamp the state at their next native invocation.
    // Use a valid constructor state, then restore every native field below.
    ActorPosePlayer result(rom, std::move(art), 0, fields.variant);
    result.actor() = fields;
    return result;
}
} // namespace
ActorInstance::ActorInstance(const Rom &rom, const QByteArray &record)
    : rom_(rom), record_(record), pose_(player(rom, record)) {}
ActorInstance ActorInstance::create(const Rom &rom, const LordlyActorSelection &selection, int mode,
                                    const Height &terrain) {
    Context context{rom, terrain, {}, {}};
    LordlyActorRecord record{};
    require(lordly_actor_construct(&record, &selection, mode, resolve, height, &context), context);
    ActorInstance result(rom, bytes(record));
    if (result.pose_.advance() != LORDLY_POSE_OK)
        throw Error(result.pose_.error());
    return result;
}
void ActorInstance::select(const LordlyActorSelection &selection) {
    if (pose_.suspended())
        throw Error("Finish the actor's current pose invocation before changing State");
    Context context{rom_, {}, {}, {}};
    auto next = raw(record());
    int advance = 0;
    require(lordly_actor_select(&next, &selection, resolve, &context, &advance), context);
    if (!advance)
        return;
    ActorInstance replacement(rom_, bytes(next));
    if (replacement.pose_.advance() != LORDLY_POSE_OK)
        throw Error(replacement.pose_.error());
    *this = std::move(replacement);
}
QByteArray ActorInstance::record() const {
    auto result = raw(record_);
    lordly_actor_write_pose(&result, &pose_.actor());
    return bytes(result);
}
void ActorInstance::setSlot(int slot) {
    if (slot < 0 || slot >= 28)
        throw Error("Actor slot is outside 0..27");
    put32(record_, 0xe4, quint32(slot));
}
void ActorInstance::setOpacity(quint8 value) {
    put8(record_, 0x13c, value);
}
void ActorInstance::setTint(quint8 red, quint8 green, quint8 blue) {
    put8(record_, 0x142, red);
    put8(record_, 0x143, green);
    put8(record_, 0x144, blue);
}
void ActorInstance::setChannel(quint8 channel) {
    put8(record_, 0x13e, channel);
}
void ActorInstance::setUniformScale(float value) {
    if (!std::isfinite(value))
        throw Error("Actor scale must be finite");
    for (int i = 0; i < 3; ++i)
        put32(record_, 0x104 + i * 4, std::bit_cast<quint32>(value));
}
void ActorInstance::addHeight(float step) {
    const float base = pose_.actor().position.y + step;
    const bool secondary = !(u8(record_, 0x145) & 4);
    const float height = secondary ? std::bit_cast<float>(u32(record_, 0x128)) + step : 0;
    if (!std::isfinite(base) || (secondary && !std::isfinite(height)))
        throw Error("Actor height movement produced a nonfinite coordinate");
    pose_.actor().position.y = base;
    if (secondary)
        put32(record_, 0x128, std::bit_cast<quint32>(height));
}
} // namespace lordly
