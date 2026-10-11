#include "rebuild/animation_overrides.h"
#include <QJsonArray>
namespace lordly {
QByteArray rebuildAnimationOverrides(const Rom &rom, const AnimationOverrides &model,
                                     const ClassData &classes, const Compiler &compiler,
                                     const QString &directory) {
    model.image().requireUnchanged(rom);
    if (!model.available())
        throw Error(model.unavailableReason());
    const auto definition = AnimationOverrideImage::definition(),
               layout = definition["layout"].toObject();
    const bool installed = !model.entries().isEmpty();
    auto result = rom.bytes();
    auto assemble = [&](const QString &name) {
        const auto spec = definition["sources"].toObject()[name].toObject();
        QMap<QString, quint32> symbols;
        const auto bindings = spec["symbols"].toObject();
        for (auto it = bindings.begin(); it != bindings.end(); ++it)
            symbols[it.key()] = quint32(it.value().toInteger());
        const auto file = spec["file"].toString().section('/', -1);
        const auto source = QString(".equ LC_INSTALLED,%1\n").arg(installed ? 1 : 0).toLatin1() +
                            AnimationOverrideImage::source(file);
        const auto bytes =
            compiler.assembleOwner(source, directory, "animation_override_" + name,
                                   quint32(spec["address"].toInteger()), symbols, true);
        if (spec.contains("sha256") && (bytes.size() != spec["size"].toInt() ||
                                        sha256(bytes) != spec["sha256"].toString().toLatin1()))
            throw Error(
                "Compiled animation override source differs from its accepted instructions: " +
                name);
        return bytes;
    };
    const auto hook = assemble("hook"), returnCode = assemble("return"),
               carriers = assemble("carriers");
    for (const auto &value : definition["sites"].toArray()) {
        const auto site = value.toObject();
        const auto name = site["name"].toString();
        QByteArray code;
        if (name == "hook")
            code = hook;
        else if (name == "return")
            code = returnCode;
        else
            code = carriers.mid(name.mid(7).toInt() * 4, 4);
        if (code.size() != site["size"].toInt() ||
            sha256(code) != site[installed ? "activeSha256" : "retailSha256"].toString().toLatin1())
            throw Error("Compiled animation override site differs from its source: " + name);
        result.replace(site["offset"].toInt(), code.size(), code);
    }
    QByteArray module(layout["moduleSize"].toInt(), char(-1)),
        bootstrap(layout["bootstrapSize"].toInt(), 0);
    if (installed) {
        bootstrap = assemble("bootstrap");
        const auto authored = model.moduleData(classes);
        module = compiler.compileData(authored, directory, "animation_override_data");
        if (module != authored)
            throw Error("Compiled animation override data differ from the project");
        const auto entry = assemble("entry"), lookup = assemble("lookup"),
                   modeReturn = assemble("mode-return");
        if (entry.size() != 8 || lookup.size() > 0x200 || modeReturn.size() > 0x80)
            throw Error("Animation override instructions exceed their module regions");
        module.replace(0x40, entry.size(), entry);
        module.replace(0x180, modeReturn.size(), modeReturn);
        module.replace(0x200, lookup.size(), lookup);
    }
    result.replace(layout["bootstrapZ64"].toInt(), bootstrap.size(), bootstrap);
    result.replace(layout["moduleZ64"].toInt(), module.size(), module);
    const auto readback = AnimationOverrideImage::inspect(Rom::fromBytes(result));
    if (!readback.recognized() || readback.version() != (installed ? 3 : 0))
        throw Error("Animation override export failed readback: " + readback.reason());
    return result;
}
} // namespace lordly
