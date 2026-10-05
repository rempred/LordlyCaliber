#include "rebuild/display_tools.h"
#include <QFile>
namespace lordly {
namespace {
QByteArray source(const QString &name) {
    QFile file(":/game/tools/" + name);
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing display source: " + name);
    return file.readAll();
}
void rebuildDisplay(const Project &project, ToolLabel kind, const Compiler &compiler,
                    const QString &directory, BuildResult &result) {
    const auto &setting = project.tools()->displayTool(kind);
    if (!setting.enabled() && !setting.modified())
        return;
    const auto original = DisplayTool::load(project.rom(), kind);
    if (!original.available())
        throw Error(original.unavailableReason());
    const bool legacy = original.installation() == DisplayTool::Installation::Legacy;
    const auto definition = DisplayTool::definition(kind);
    const auto name = definition["name"].toString();
    const auto sites = definition["sites"].toArray();
    QMap<QString, quint32> symbols{
        {"TOOL_NATIVE_DRAW", quint32(definition["nativeDraw"].toInteger())},
        {"TOOL_INVALIDATE_ICACHE", 0x800900c0},
        {"TOOL_INVALIDATE_DCACHE", 0x80090010}};
    for (const auto &value : sites) {
        const auto site = value.toObject();
        if (site["name"] == "source-loader")
            symbols["TOOL_BOOTSTRAP"] = quint32(site["ram"].toInteger());
        if (site["name"] == "source-module")
            symbols["TOOL_ENTRY"] = quint32(site["ram"].toInteger()) + 0x100;
    }
    auto image = result.image;
    for (const auto &value : sites) {
        const auto site = value.toObject();
        const int at = site["rom"].toInt(), size = site["size"].toInt();
        const bool module = site["name"] == "source-module";
        if (module && legacy && !setting.enabled())
            continue;
        if (image.mid(at, size) != project.rom().bytes().mid(at, size))
            throw Error("Another rebuild owner overlaps the " + name + " display");
        if (module && legacy &&
            sha256(image.mid(at, size)) != site["retailSha256"].toString().toLatin1())
            throw Error("The " + name + " source-module allocation is occupied");
        QByteArray assembly;
        if (site["name"] == "native-hook")
            assembly = QString(".set TOOL_RETAIL,%1\n.set TOOL_ALIGNMENT,%2\n")
                           .arg(setting.enabled() ? 0 : 1)
                           .arg(kind == ToolLabel::Alignment ? 1 : 0)
                           .toLatin1() +
                       source(site["source"].toString());
        else if (!setting.enabled())
            assembly = QString(".space %1,%2\n").arg(size).arg(module ? 255 : 0).toLatin1();
        else {
            assembly = source(site["source"].toString());
            if (module) {
                const auto texture = original.installation() == DisplayTool::Installation::Native
                                         ? project.rom().bytes().mid(at + 0x520, 512)
                                         : displayToolTexture(Rom::fromBytes(image), kind);
                for (int i = 0; i < texture.size(); i += 16) {
                    assembly += ".byte ";
                    for (int j = 0; j < 16; ++j)
                        assembly += (j ? "," : "") + QByteArray::number(quint8(texture[i + j]));
                    assembly += '\n';
                }
                assembly += source(name + "-templates.s");
            }
        }
        const auto built =
            compiler.assembleOwner(assembly, directory, name + "-" + site["name"].toString(),
                                   quint32(site["ram"].toInteger()), symbols, true);
        DisplayTool::validateCompiled(site, built, setting.enabled());
        image.replace(at, size, built);
    }
    if (legacy)
        for (const auto &value : sites) {
            const auto site = value.toObject();
            if (site["name"] != "source-module")
                continue;
            const int at = site["legacyRom"].toInt(), size = site["legacyBytes"].toInt();
            if (image.mid(at, size) != project.rom().bytes().mid(at, size))
                throw Error("Another rebuild owner overlaps the legacy " + name + " module");
            image.replace(at, size, QByteArray(size, '\xff'));
        }
    const auto reopened = DisplayTool::load(Rom::fromBytes(image), kind);
    if (reopened.enabled() != setting.enabled() ||
        (!reopened.available() && !(legacy && !setting.enabled())))
        throw Error("Rebuilt " + name + " display failed readback");
    result.image = std::move(image);
    result.report << (setting.enabled()
                          ? "Compiled " + name +
                                " display, loader and current-ROM artwork; requires an Expansion "
                                "Pak (8 MiB RAM)"
                          : "Removed " + name + " display and restored its native call");
}
} // namespace
void rebuildDisplayTools(const Project &project, const Compiler &compiler, const QString &directory,
                         BuildResult &result) {
    if (!project.tools())
        return;
    rebuildDisplay(project, ToolLabel::Luck, compiler, directory, result);
    rebuildDisplay(project, ToolLabel::Alignment, compiler, directory, result);
}
} // namespace lordly
