#include "engine/accepted_effect_code.h"
#include <QFile>
namespace lordly {
int installAcceptedEffectCode(EffectMachine &machine, const QByteArray &rom) {
    static const QByteArray compiled = [] {
        QFile file(":/generated/accepted-effects.bin");
        if (!file.open(QIODevice::ReadOnly)) throw Error("Missing compiled accepted effect C");
        return file.readAll();
    }();
    if (compiled.size() < 44 || compiled.first(8) != "LCCODE01")
        throw Error("Invalid compiled effect C header");
    const auto count = u32(compiled, 8);
    if (!count || count > 4096) throw Error("Invalid compiled effect C count");
    qsizetype at = 44;
    int installed = 0;
    for (quint32 i = 0; i < count; ++i) {
        if (at + 16 > compiled.size()) throw Error("Truncated compiled effect C directory");
        const auto start = u32(compiled, at), address = u32(compiled, at + 4),
                   size = u32(compiled, at + 8), code = u32(compiled, at + 12);
        at += 16;
        if (!size || (size & 3) || code > 1 || quint64(at) + size > quint64(compiled.size()) ||
            quint64(start) + size > quint64(rom.size()))
            throw Error("Invalid compiled effect C range");
        const auto bytes = compiled.mid(at, size);
        const auto physical = machine.physical(address, size);
        // A project ROM can already contain mod hooks or changed tables. Never
        // replace them with the retail baseline, or mount an unloaded overlay.
        if (rom.mid(start, size) == bytes && machine.memory.mid(physical, size) == bytes) {
            machine.mount(address, bytes, code != 0);
            ++installed;
        }
        at += size;
    }
    if (at != compiled.size()) throw Error("Unexpected compiled effect C tail");
    return installed;
}
}
