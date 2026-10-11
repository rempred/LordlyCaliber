#include "engine/actor_turn.h"
namespace lordly {
ActorTurnJob::ActorTurnJob(QByteArray record) : record_(std::move(record)) {
    if (!record_.isEmpty() && record_.size() != 10)
        throw Error("Actor turn job must contain ten bytes");
}
void ActorTurnJob::start(const Rom &rom, int from, int to, int direction, qint32 duration) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Actor turns currently require US revision 0");
    if (from < 0 || from > 3 || to < 0 || to > 3 || (direction != -1 && direction != 1))
        throw Error("Actor turn requires two cardinal directions and a step of -1 or 1");
    // RAM 80239190: four halfwords in the Actor-scene slab.
    const int first = qint16(u16(rom.bytes(), 0x2a89c0 + from * 2)),
              last = qint16(u16(rom.bytes(), 0x2a89c0 + to * 2));
    if (first < 0 || first >= 12 || last < 0 || last >= 12)
        throw Error("Actor turn phase table is outside 0..11");
    int distance = direction == -1 ? last - first : first - last;
    if (distance < 0)
        distance += 12;
    if (!distance)
        throw Error("Actor turn has a zero phase distance");
    const qint32 cadence = ((duration / 3) * 3) / distance;
    QByteArray next(10, 0);
    put16(next, 0, quint16(first));
    put16(next, 2, quint16(distance));
    put16(next, 4, quint16(cadence));
    put16(next, 8, quint16(-direction));
    record_ = std::move(next);
}
std::optional<int> ActorTurnJob::advance(const Rom &rom) {
    if (!present())
        return {};
    auto next = record_;
    std::optional<int> facing;
    if (u16(next, 6) == 0) {
        put16(next, 2, quint16(u16(next, 2) - 1U));
        int phase = qint16(quint16(u16(next, 0) + u16(next, 8)));
        if (phase < 0)
            phase += 12;
        else if (phase >= 12)
            phase -= 12;
        if (phase < 0 || phase >= 12)
            throw Error("Actor turn phase is outside its table");
        put16(next, 0, quint16(phase));
        put16(next, 6, u16(next, 4));
        facing = qint16(u16(rom.bytes(), 0x2a89c8 + phase * 2));
        if (u16(next, 2) == 0)
            next.clear();
    }
    if (!next.isEmpty())
        put16(next, 6, quint16(u16(next, 6) - 1U));
    record_ = std::move(next);
    return facing;
}
} // namespace lordly
