#include "rebuild/accepted_rom.h"
#include "engine/rom.h"
#include <QFile>
#include <QVector>
#include <algorithm>
namespace lordly {
namespace {
struct Region {
    quint32 start;
    QByteArray bytes;
    bool code;
};
const QVector<Region> &acceptedRegions() {
    static const auto regions = [] {
        QFile file(":/generated/accepted-rom.bin");
        if (!file.open(QIODevice::ReadOnly)) throw Error("Missing compiled accepted C baseline");
        const auto compiled = file.readAll();
        if (compiled.size() < 44 || !compiled.startsWith("LCCODE01"))
            throw Error("Invalid accepted C baseline header");
        const auto count = u32(compiled, 8);
        if (!count || count > 16384) throw Error("Invalid accepted C baseline count");
        QVector<Region> result;
        result.reserve(count);
        qsizetype at = 44;
        for (quint32 i = 0; i < count; ++i) {
            if (at + 16 > compiled.size()) throw Error("Truncated accepted C baseline directory");
            const auto start = u32(compiled, at), size = u32(compiled, at + 8), code = u32(compiled, at + 12);
            at += 16;
            if (!size || code > 1 || (code && (size & 3)) || quint64(at) + size > quint64(compiled.size()) ||
                (!result.isEmpty() && start < result.last().start))
                throw Error("Invalid accepted C baseline allocation");
            result.append({start, compiled.mid(at, size), code != 0});
            at += size;
        }
        if (at != compiled.size()) throw Error("Unexpected accepted C baseline tail");
        return result;
    }();
    return regions;
}
}
QByteArray acceptedRomBytes(int offset, int size) {
    const auto &regions = acceptedRegions();
    auto it = std::upper_bound(regions.cbegin(), regions.cend(), offset,
                               [](int at, const Region &r) { return qint64(at) < r.start; });
    if (it == regions.cbegin()) throw Error("Missing accepted C region");
    --it;
    if (size < 0 || qint64(offset) + size > it->start + it->bytes.size())
        throw Error("Accepted C region exceeds owner");
    return it->bytes.mid(offset - it->start, size);
}
AcceptedRomCoverage applyAcceptedRom(QByteArray &image) {
    AcceptedRomCoverage result;
    for (const auto &r : acceptedRegions()) {
        if (r.start + r.bytes.size() > image.size()) throw Error("Invalid accepted C baseline allocation");
        if (image.mid(r.start, r.bytes.size()) == r.bytes) {
            image.replace(r.start, r.bytes.size(), r.bytes);
            if (r.code) ++result.codeRegions;
            else ++result.dataRegions;
        } else {
            ++result.retainedRegions;
        }
    }
    return result;
}
}
