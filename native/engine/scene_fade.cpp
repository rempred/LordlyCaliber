#include "engine/scene_fade.h"
namespace lordly {
// Portable reconstruction of ROM2838AC/282508/2827AC, with explicit allocation
// and scheduler ownership instead of the native global pointer.
SceneFade::SceneFade(QByteArray record, bool active) : record_(std::move(record)), active_(active) {
    if (!record_.isEmpty() && record_.size() != 12)
        throw Error("Screen fade requires a 12-byte record");
    if (active && !present())
        throw Error("Screen fade slot requires its effect object");
}
int SceneFade::remaining() const {
    return present() ? qint16(u16(record_, 0)) : 0;
}
std::array<quint8, 4> SceneFade::rgba() const {
    if (!present())
        return {};
    return {u8(record_, 4), u8(record_, 5), u8(record_, 6), u8(record_, 8)};
}
bool SceneFade::start(qint32 duration, qint32 red, qint32 green, qint32 blue, qint32 from,
                      qint32 to) {
    const bool registerSlot = !present() || u8(record_, 7) == 1;
    if (!present())
        record_ = QByteArray(12, 0);
    put8(record_, 7, registerSlot ? 0 : 2);
    if (registerSlot)
        active_ = true;
    put16(record_, 0, quint16(duration));
    put16(record_, 2, quint16(duration));
    put8(record_, 4, quint8(red));
    put8(record_, 5, quint8(green));
    put8(record_, 6, quint8(blue));
    put8(record_, 9, quint8(to));
    if (from != -1)
        put8(record_, 8, quint8(from));
    put8(record_, 10, u8(record_, 8));
    return registerSlot;
}
void SceneFade::markRelease() {
    if (present())
        put8(record_, 7, 1);
}
bool SceneFade::advance() {
    if (!active_ || !present())
        return false;
    if (u8(record_, 7) == 1)
        return true;
    auto next = record_;
    if (u8(next, 7) == 2)
        put8(next, 7, 0);
    if (remaining() > 0) {
        const int divisor = qint16(u16(next, 2));
        if (!divisor)
            throw Error("Screen fade has a positive countdown with zero duration");
        const int count = remaining() - 1, to = u8(next, 9), from = u8(next, 10);
        const int alpha = to + (from - to) * count / divisor;
        put16(next, 0, quint16(count));
        put8(next, 8, quint8(alpha));
    }
    record_ = std::move(next);
    return false;
}
void SceneFade::cleanup() {
    active_ = false;
    if (!present())
        return;
    put16(record_, 0, 0);
    if (u8(record_, 7) == 1)
        record_.clear();
}
bool SceneFade::service(quint32 opcode, const QVector<quint32> &a, int &value) {
    if (opcode == 0x1b) {
        if (a.size() != 6)
            throw Error("Screen fade command is truncated");
        start(qint32(a[0]), qint32(a[1]), qint32(a[2]), qint32(a[3]), qint32(a[4]), qint32(a[5]));
    } else if (opcode == 0x7d || opcode == 0x7e) {
        if (!a.isEmpty())
            throw Error("Screen fade release has unexpected operands");
        markRelease();
    } else if (opcode == 0x7f) {
        if (a.size() != 2)
            throw Error("Screen fade query is truncated");
        value = remaining();
    } else
        return false;
    return true;
}
} // namespace lordly
