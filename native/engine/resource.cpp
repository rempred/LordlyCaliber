#include "engine/resource.h"
namespace lordly {
Resource Resource::load(const Rom &rom, qsizetype offset, qsizetype allocation) {
    Resource r;
    r.romOffset = offset;
    if (offset < 0 || allocation < 8 || offset > rom.bytes().size() ||
        allocation > rom.bytes().size() - offset)
        throw Error("Resource allocation outside ROM");
    r.originalSlot = rom.bytes().mid(offset, allocation);
    auto payload = u32(r.originalSlot, 0), size = u32(r.originalSlot, 4);
    if (payload < 4 || quint64(payload) + 4 > quint64(allocation))
        throw Error("Resource payload exceeds its allocation");
    r.capacity = allocation - 8;
    r.original = decodeLzss(r.originalSlot.mid(8, payload - 4), size);
    r.decoded = r.original;
    return r;
}
QByteArray Resource::container(const QByteArray &compiled) const {
    auto encoded = encodeLzss(compiled);
    auto stored = capacity;
    if (encoded.size() > capacity || stored + 8 > originalSlot.size())
        throw Error(QString("Resource needs %1 bytes; its current allocation holds %2")
                        .arg(encoded.size() + 8)
                        .arg(originalSlot.size()));
    if (decodeLzss(encoded, compiled.size()) != compiled)
        throw Error("Resource encoder readback failed");
    auto out = originalSlot;
    put32(out, 0, quint32(stored + 4));
    put32(out, 4, quint32(compiled.size()));
    out.replace(8, stored, QByteArray(stored, 3));
    out.replace(8, encoded.size(), encoded);
    return out;
}
} // namespace lordly
