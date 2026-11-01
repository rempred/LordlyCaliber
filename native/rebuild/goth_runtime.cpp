#include "rebuild/goth_runtime.h"
#include "engine/goth_runtime.h"
#include <QJsonArray>
namespace lordly {
void rebuildGothRuntime(const Project &project, const Compiler &compiler,
                        const QString &directory, BuildResult &result) {
    const auto &rules = project.tools()->gameRules();
    if (!rules.unavailable("goth").isEmpty() ||
        (rules.value("reserveXp") != 0 && !rules.unavailable("reserve").isEmpty()))
        throw Error("Settlement or reserve XP code is not recognized");
    const auto catalog = GothRuntime::definition();
    const auto retailOwner = catalog["retail"].toObject();
    const auto original = compiler.compileQualifiedOwner(retailOwner, directory, "settlement_retail");
    const auto before = project.rom().bytes().mid(GothRuntime::offset, GothRuntime::size);
    if (result.image.mid(GothRuntime::offset, GothRuntime::size) != before)
        throw Error("Another rebuild owner overlaps settlement code");
    const auto prior = GothRuntime::read(project.rom());
    if (!prior) {
        auto canonical = before;
        // GameRules already authenticates the legacy arithmetic/loader and
        // reserve call. Require every other original instruction as well.
        canonical.replace(0x1399c0 - GothRuntime::offset, 244,
                          original.code.mid(0x1399c0 - GothRuntime::offset, 244));
        canonical.replace(0x139afc - GothRuntime::offset, 4,
                          original.code.mid(0x139afc - GothRuntime::offset, 4));
        if (canonical != original.code) throw Error("Unrecognized settlement code outside its legacy adapter");
        for (auto it = original.tables.cbegin(); it != original.tables.cend(); ++it)
            if (project.rom().bytes().mid(it.key(), it.value().size()) != it.value())
                throw Error("Unrecognized original settlement switch table");
    }
    const int low = rules.value("gothLowDivisor"), high = rules.value("gothHighDivisor");
    const bool reserve = rules.value("reserveXp") != 0;
    auto built = original;
    if (low != 25 || high != 50 || reserve) {
        built = compiler.compileQualifiedOwner(catalog[reserve ? "reserve" : "plain"].toObject(),
                                               directory, "settlement_configured");
        QByteArray header(16, 0);
        header.replace(0, 8, "LCGOTH01");
        put16(header, 8, low); put16(header, 10, high); put32(header, 12, reserve);
        built.code += header;
    }
    if (built.code.size() != GothRuntime::size) throw Error("Settlement C exceeds its allocation");
    result.image.replace(GothRuntime::offset, GothRuntime::size, built.code);
    for (auto it = built.tables.cbegin(); it != built.tables.cend(); ++it) {
        if (result.image.mid(it.key(), it.value().size()) != project.rom().bytes().mid(it.key(), it.value().size()))
            throw Error("Another rebuild owner overlaps the settlement switch table");
        result.image.replace(it.key(), it.value().size(), it.value());
    }
    result.report << "Compiled complete settlement C, Goth divisors and the reserve-XP loader";
}
}
