#include "engine/actor_divider.h"
#include "game/director/actor_divider.h"
#include <bit>
#include <cmath>
namespace lordly {
void ActorDivider::supply(QByteArray record) {
    if (!record.isEmpty() &&
        (record.size() != 12 || !std::isfinite(std::bit_cast<float>(u32(record, 0))) ||
         !std::isfinite(std::bit_cast<float>(u32(record, 4)))))
        throw Error("Actor divider requires a twelve-byte record with finite coefficients");
    record_ = std::move(record);
}
void ActorDivider::start(const QVector<quint32> &args) {
    if (args.size() != 6)
        throw Error("Actor divider command is truncated");
    if (record_.isEmpty())
        record_ = QByteArray(12, 0);
    lordly_actor_divider_begin(reinterpret_cast<uint8_t *>(record_.data()), args.constData());
}
std::optional<quint8> ActorDivider::channel(float x, float z) const {
    if (record_.isEmpty())
        return {};
    if (!std::isfinite(x) || !std::isfinite(z))
        throw Error("Actor divider requires finite coordinates");
    return lordly_actor_divider_channel(reinterpret_cast<const uint8_t *>(record_.constData()), x,
                                        z);
}
} // namespace lordly
