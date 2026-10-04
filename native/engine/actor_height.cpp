#include "engine/actor_height.h"
#include <bit>
#include <cmath>
namespace lordly {
ActorHeightJob::ActorHeightJob(QByteArray record) : record_(std::move(record)) {
    if (!record_.isEmpty() && record_.size() != 12)
        throw Error("Actor height job must contain 12 bytes");
}
void ActorHeightJob::start(const Rom &rom, qint32 interval, qint32 height, qint32 duration,
                           std::optional<float> baseY) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Actor height currently requires US revision 0");
    float step = float(height);
    if (qint16(interval) == -1) {
        if (!baseY || !std::isfinite(*baseY))
            throw Error("Target-height movement requires the actor's finite base height");
        const auto read = [&](qsizetype at) {
            return std::bit_cast<double>((quint64(u32(rom.bytes(), at)) << 32) |
                                         u32(rom.bytes(), at + 4));
        };
        // Actor-scene slab RAM 80239180/88. These are not boot-code addresses.
        const double positive = read(0x2a89b0), negative = read(0x2a89b8);
        if (!std::isfinite(positive) || !std::isfinite(negative) || positive < 0 || negative > 0)
            throw Error("Actor height tolerances are invalid");
        const float difference = step - *baseY;
        if (double(difference) < positive && double(difference) > negative) {
            record_.clear();
            return;
        }
        if (!duration)
            throw Error("Target-height movement has a zero divisor");
        step = difference / float(duration);
        if (!std::isfinite(step))
            throw Error("Target-height movement exceeds the finite range");
    }
    auto next = present() ? record_ : QByteArray(12, 0);
    put16(next, 0, quint16(interval));
    put16(next, 2, 0);
    put16(next, 4, quint16(duration));
    put32(next, 8, std::bit_cast<quint32>(step));
    record_ = std::move(next);
}
std::optional<float> ActorHeightJob::advance() {
    if (!present())
        return {};
    const int progress = qint16(quint16(u16(record_, 2) + 1));
    if (progress == qint16(u16(record_, 4))) {
        record_.clear();
        return {};
    }
    const int interval = qint16(u16(record_, 0));
    if (!interval)
        throw Error("Actor height job has a zero step interval");
    const bool applies = interval == -1 || progress % interval == interval - 1;
    const float step = std::bit_cast<float>(u32(record_, 8));
    if (applies && !std::isfinite(step))
        throw Error("Actor height job contains a nonfinite step");
    put16(record_, 2, quint16(progress));
    return applies ? std::optional<float>(step) : std::nullopt;
}
} // namespace lordly
