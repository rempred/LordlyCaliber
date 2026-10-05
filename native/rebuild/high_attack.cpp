#include "rebuild/high_attack.h"
namespace lordly {
void rebuildHighAttack(const Project &project, const Compiler &compiler, const QString &directory,
                       BuildResult &result) {
    if (!project.tools())
        return;
    const auto &setting = project.tools()->highAttack();
    if (!setting.enabled() && !setting.modified())
        return;
    const auto original = HighAttack::load(project.rom());
    if (!original.available())
        throw Error(original.unavailableReason());
    const auto definition = HighAttack::definition();
    const auto bindings = definition["symbols"].toObject();
    QMap<QString, quint32> symbols;
    for (auto i = bindings.begin(); i != bindings.end(); ++i)
        symbols[i.key()] = quint32(i.value().toInteger());
    auto image = result.image;
    for (const auto &value : definition["sites"].toArray()) {
        const auto site = value.toObject();
        const int at = site["rom"].toInt(), size = site["size"].toInt();
        if (image.mid(at, size) != project.rom().bytes().mid(at, size))
            throw Error("Another rebuild owner overlaps High Attack: " + site["name"].toString());
        const auto built =
            compiler.assembleOwner(HighAttack::assembly(site, setting.enabled()), directory,
                                   "high-attack-" + site["name"].toString(),
                                   quint32(site["ram"].toInteger()), symbols, true);
        HighAttack::validateCompiled(site, built, setting.enabled());
        image.replace(at, size, built);
    }
    const auto reopened = HighAttack::load(Rom::fromBytes(image));
    if (!reopened.available() || reopened.enabled() != setting.enabled())
        throw Error("Rebuilt High Attack setting failed readback");
    result.image = std::move(image);
    result.report << (setting.enabled() ? "Compiled experimental High Attack stream split; "
                                          "requires Expansion Pak (8 MiB) and "
                                          "Project64 Interpreter. Full-battle validation pending."
                                        : "Removed High Attack and restored retail code");
}
} // namespace lordly
