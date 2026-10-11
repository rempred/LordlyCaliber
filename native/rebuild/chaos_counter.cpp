#include "rebuild/chaos_counter.h"
#include "engine/tool_artwork.h"
#include <QFile>
namespace lordly {
namespace {
QByteArray source(const QString &name) {
    QFile file(":/game/tools/" + name);
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing Chaos Frame source: " + name);
    return file.readAll();
}
} // namespace
void rebuildChaosCounter(const Project &project, const Compiler &compiler, const QString &directory,
                         BuildResult &result) {
    if (!project.tools())
        return;
    const auto &setting = project.tools()->chaosCounter();
    if (!setting.enabled() && !setting.modified())
        return;
    const auto original = ChaosCounter::load(project.rom());
    if (!original.available())
        throw Error(original.unavailableReason());
    const bool legacy = original.installation() == ChaosCounter::Installation::Legacy;
    auto image = result.image;
    for (const auto &value : ChaosCounter::sites()) {
        const auto site = value.toObject();
        const int at = site["rom"].toInt(), size = site["size"].toInt();
        const auto name = site["name"].toString();
        const bool module = name == "source-module";
        if (module && legacy && !setting.enabled())
            continue;
        if (image.mid(at, size) != project.rom().bytes().mid(at, size))
            throw Error("Another rebuild owner overlaps the Chaos Frame counter");
        if (module && legacy &&
            sha256(image.mid(at, size)) != site["retailSha256"].toString().toLatin1())
            throw Error("Chaos Frame source-module allocation is occupied");
        QByteArray assembly;
        if (name == "native-hook")
            assembly = QByteArray(".set CF_RETAIL,") + (setting.enabled() ? "0\n" : "1\n") +
                       source(site["source"].toString());
        else if (!setting.enabled())
            assembly = QString(".space %1,%2\n").arg(size).arg(module ? 255 : 0).toLatin1();
        else {
            assembly = source(site["source"].toString());
            if (module) {
                // A recognized installed module owns a bounded user-editable
                // texture. Preserve it while rebuilding its code from source.
                const auto title = original.installation() == ChaosCounter::Installation::Native
                                       ? project.rom().bytes().mid(at + 0x700, 384)
                                       : chaosFrameTitle(Rom::fromBytes(image));
                for (int i = 0; i < title.size(); i += 16) {
                    assembly += ".byte ";
                    for (int j = 0; j < 16; ++j)
                        assembly += (j ? "," : "") + QByteArray::number(quint8(title[i + j]));
                    assembly += '\n';
                }
                assembly += source("chaos-title-list.s");
                assembly += ".space 0x600,255\n";
            }
        }
        const QMap<QString, quint32> symbols{{"CF_SUBMIT_TASK", 0x80090140},
                                             {"CF_RETURN", 0x80093b84},
                                             {"CF_BOOTSTRAP", 0x800a4a78},
                                             {"CF_CONTINUATION", 0x800a038c},
                                             {"CF_ENTRY", 0x80420100},
                                             {"CF_INVALIDATE_ICACHE", 0x800900c0},
                                             {"CF_INVALIDATE_DCACHE", 0x80090010}};
        const auto built = compiler.assembleOwner(assembly, directory, name,
                                                  quint32(site["ram"].toInteger()), symbols, true);
        ChaosCounter::validateCompiled(site, built, setting.enabled());
        image.replace(at, size, built);
    }
    if (legacy) {
        // Only the authenticated legacy module is reclaimed; unrelated tail bytes survive.
        const int at = 0x2790000, size = 0xa00;
        if (image.mid(at, size) != project.rom().bytes().mid(at, size))
            throw Error("Another rebuild owner overlaps the legacy Chaos Frame module");
        image.replace(at, size, QByteArray(size, '\xff'));
    }
    const auto reopened = ChaosCounter::load(Rom::fromBytes(image));
    // Removing a legacy installation does not claim its replacement slot.
    // Foreign bytes there may correctly leave the new counter unavailable.
    if (reopened.enabled() != setting.enabled() ||
        (!reopened.available() && !(legacy && !setting.enabled())))
        throw Error("Rebuilt Chaos Frame counter failed readback");
    result.image = std::move(image);
    result.report << (setting.enabled()
                          ? "Compiled Chaos Frame counter, loader and current-ROM label; requires "
                            "an Expansion Pak (8 MiB RAM)"
                          : "Removed Chaos Frame counter and restored its original call site");
}
} // namespace lordly
