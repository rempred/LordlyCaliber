#include "rebuild/accepted_rom.h"
#include "engine/rom.h"
#include <QFile>
namespace lordly {
AcceptedRomCoverage applyAcceptedRom(QByteArray &image) {
    static const QByteArray compiled = [] {
        QFile file(":/generated/accepted-rom.bin");
        if (!file.open(QIODevice::ReadOnly)) throw Error("Missing compiled accepted C baseline");
        return file.readAll();
    }();
    if (compiled.size() < 44 || compiled.first(8) != "LCCODE01")
        throw Error("Invalid accepted C baseline header");
    const auto count = u32(compiled, 8);
    if (!count || count > 16384) throw Error("Invalid accepted C baseline count");
    AcceptedRomCoverage result;
    qsizetype at = 44;
    for (quint32 i = 0; i < count; ++i) {
        if (at + 16 > compiled.size()) throw Error("Truncated accepted C baseline directory");
        const auto start = u32(compiled, at), size = u32(compiled, at + 8), code = u32(compiled, at + 12);
        at += 16;
        if (!size || code > 1 || (code && (size & 3)) ||
            quint64(at) + size > quint64(compiled.size()) ||
            quint64(start) + size > quint64(image.size()))
            throw Error("Invalid accepted C baseline allocation");
        const auto bytes = compiled.mid(at, size);
        // Never replace a mod's hook, altered constant or generated runtime with
        // retail code. Unconverted gaps and original owner suffixes stay intact.
        if (image.mid(start, size) == bytes) {
            image.replace(start, size, bytes);
            if (code) ++result.codeRegions;
            else ++result.dataRegions;
        } else {
            ++result.retainedRegions;
        }
        at += size;
    }
    if (at != compiled.size()) throw Error("Unexpected accepted C baseline tail");
    return result;
}
}
