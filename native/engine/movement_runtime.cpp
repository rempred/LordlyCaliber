#include "engine/movement_runtime.h"
#include "engine/class_data.h"
#include "engine/promotion.h"
#include "engine/resource.h"
#include <QJsonDocument>
#include <QSet>
namespace lordly {
std::optional<MovementPlacement> MovementPlacement::read(const Rom &rom) {
    const auto &b = rom.bytes();
    if (b.size() < descriptorOffset + descriptorSize || b.mid(descriptorOffset, 8) != "LCMOVE01")
        return {};
    const auto h = b.mid(descriptorOffset, descriptorSize);
    if (u32(h, 8) != 1 || crc32(h.first(60)) != u32(h, 60))
        throw Error("Movement descriptor checksum failed");
    MovementPlacement p;
    p.offset = u32(h, 12);
    p.address = u32(h, 16);
    p.size = u32(h, 20);
    p.originalDataCrc = u32(h, 28);
    p.originalClassHeaderCrc = u32(h, 32);
    const auto classes = b.mid(ClassPlacement::descriptorRomOffset, ClassPlacement::descriptorSize);
    if (classes.size() != 128 || classes.first(8) != "LC05CLAS" ||
        crc32(classes.first(124)) != u32(classes, 124) || p.offset < u32(classes, 12) ||
        p.address != u32(classes, 24) + p.offset - u32(classes, 12) || p.size < 64 ||
        quint64(p.offset) + p.size > quint64(b.size()) ||
        quint64(p.address) + p.size > u32(classes, 48))
        throw Error("Movement data is outside its permanent allocation");
    p.data = b.mid(p.offset, p.size);
    if (crc32(p.data) != u32(h, 24) || p.data.first(8) != "LCMOVDAT")
        throw Error("Movement data checksum failed");
    const auto j = u32(p.data, 8), n = u32(p.data, 12);
    p.patches = u32(p.data, 16);
    p.patchCount = u32(p.data, 20);
    if (j < 64 || quint64(j) + n > p.size || p.patches < 64 ||
        quint64(p.patches) + quint64(p.patchCount) * 12 > p.size || u32(p.data, 56) < 64 ||
        quint64(u32(p.data, 56)) + quint64(p.patchCount) * 4 > p.size)
        throw Error("Invalid movement metadata bounds");
    const auto json = QJsonDocument::fromJson(p.data.mid(j, n));
    if (!json.isObject())
        throw Error("Invalid movement metadata");
    p.model = json.object();
    QSet<quint32> offsets;
    for (quint32 i = 0; i < p.patchCount; i++) {
        const auto a = p.patches + i * 12, at = u32(p.data, a);
        if ((at & 3) || at < 0x400 || quint64(at) + 4 > quint64(b.size()) || offsets.contains(at) ||
            u32(b, at) != u32(p.data, a + 8))
            throw Error("Movement runtime owner has changed");
        offsets.insert(at);
    }
    return p;
}
QByteArray MovementPlacement::canonical(const Rom &rom) const {
    auto b = rom.bytes(), prior = data;
    for (quint32 i = 0; i < patchCount; i++) {
        const auto a = patches + i * 12;
        put32(b, u32(data, a), u32(data, a + 4));
        put32(prior, a + 4, u32(data, u32(data, 56) + i * 4));
    }
    b.replace(offset, size, prior);
    put32(b, ClassPlacement::descriptorRomOffset + 52, originalDataCrc);
    put32(b, ClassPlacement::descriptorRomOffset + 124, originalClassHeaderCrc);
    b.replace(descriptorOffset, descriptorSize, QByteArray(descriptorSize, char(-1)));
    return b;
}
QByteArray MovementPlacement::descriptor() const {
    QByteArray h(descriptorSize, 0);
    h.replace(0, 8, "LCMOVE01");
    put32(h, 8, 1);
    put32(h, 12, offset);
    put32(h, 16, address);
    put32(h, 20, data.size());
    put32(h, 24, crc32(data));
    put32(h, 28, originalDataCrc);
    put32(h, 32, originalClassHeaderCrc);
    put32(h, 60, crc32(h.first(60)));
    return h;
}
} // namespace lordly
