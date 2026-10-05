#include "engine/stage_placements.h"
namespace lordly {
QByteArray StagePlacement::resolve(QByteArray out) const {
    const bool perspective = kind == StagePlacementKind::Perspective;
    if (source.size() != (perspective ? 26 : 22))
        throw Error("Stage placement row has the wrong size");
    const int size = perspective ? 36 : 24;
    if (out.isEmpty())
        out = QByteArray(size, 0);
    if (out.size() != size)
        throw Error("Stage placement result has the wrong size");
    const int status = qint16(u16(source, 0));
    if (status == -1)
        return out;
    const auto copy = [&](int from, int to) { put16(out, to, u16(source, from)); };
    if (status == -2) {
        copy(2, 0);
        copy(4, 2);
        copy(6, 4);
        if (perspective) {
            for (int i = 0; i < 9; ++i)
                copy(8 + i * 2, 18 + i * 2);
        } else {
            for (int i = 0; i < 6; ++i)
                copy(8 + i * 2, 12 + i * 2);
            copy(20, 10);
        }
    } else if (perspective) {
        copy(0, 0);
        copy(2, 2);
        copy(4, 4);
        copy(6, 8);
        put16(out, 10, u16(source, 8) == 0x8000 ? 0 : u16(source, 8));
        copy(10, 12);
        copy(12, 6);
        copy(14, 14);
        copy(16, 16);
        put16(out, 22, 1);
    } else {
        for (int i = 0; i < 6; ++i)
            copy(i * 2, i * 2);
        put16(out, 14, 1);
    }
    return out;
}
StagePlacements StagePlacements::load(const Rom &rom, int selector) {
    rom.requireSupported();
    if (rom.revision() || selector < 0 || selector >= 80)
        throw Error("Stage placement requires a revision-0 foreground selector in 0..79");
    constexpr quint32 envelope = 0x315736 + 0x594280;
    const auto size = u32(rom.bytes(), envelope);
    if (size < 36 || size > 65536 || quint64(envelope) + 4 + size > quint64(rom.bytes().size()))
        throw Error("Stage placement resource exceeds its ROM envelope");
    const auto data = rom.bytes().mid(envelope + 4, size);
    StagePlacements result;
    for (int kind = 0; kind < 2; ++kind) {
        const int base = u16(data, 18 + kind * 2), stride = kind ? 26 : 22;
        if (base < 36 || base + 160 > data.size())
            throw Error("Stage placement directory exceeds its resource");
        int at = base + qint16(u16(data, base + selector * 2));
        auto &rows = kind ? result.perspective_ : result.orthographic_;
        while (true) {
            if (at < 36 || at + 2 > data.size())
                throw Error("Stage placement list is missing its terminator");
            if (qint16(u16(data, at)) == -1)
                break;
            if (at + stride > data.size() || rows.size() >= 1024)
                throw Error("Stage placement list exceeds its record budget");
            rows << StagePlacement{kind ? StagePlacementKind::Perspective
                                        : StagePlacementKind::Orthographic,
                                   data.mid(at, stride)};
            at += stride;
        }
    }
    return result;
}
} // namespace lordly
