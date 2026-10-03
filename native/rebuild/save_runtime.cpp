#include "rebuild/save_runtime.h"
#include <QFile>
#include <QJsonArray>
namespace lordly {
SaveRuntimeBuild buildSaveRuntime(quint32 discovery, const Compiler &compiler,
                                  const QString &directory) {
    auto source = [](const QString &name) {
        QFile file(":/game/classes/" + name);
        if (!file.open(QIODevice::ReadOnly))
            throw Error("Missing maintained save source " + name);
        return file.readAll();
    };
    SaveRuntimeBuild result;
    result.code = compiler.compileModule(source("save-format.c") + "\n" + source("save-runtime.c"),
                                         directory, "save_runtime", SavePlacement::ramAddress,
                                         {{"save_format.h", source("save_format.h")}},
                                         {{"lc_save_cache", 0x800a83b8},
                                          {"lc_save_dirty", 0x800a83bc},
                                          {"lc_discovery_flags", discovery},
                                          {"lc_legacy_discovery", 0x80196a76},
                                          {"lc_native_save_alloc", 0x80070f30},
                                          {"lc_native_save_free", 0x800712c4},
                                          {"lc_native_sram_io", 0x8008a0f0},
                                          {"lc_native_main_save", 0x8016cf64},
                                          {"lc_native_save_cleanup", 0x80074bf0},
                                          {"lc_save_callbacks", 0x800a824c},
                                          {"lc_native_options_init", 0x8016cd90},
                                          {"lc_native_options_load", 0x8016cdcc},
                                          {"lc_native_options_save", 0x8016cdf4}});
    result.placement.codeBytes = quint32(result.code.bytes.size());
    result.placement.codeChecksum = crc32(result.code.bytes);
    result.placement.validate();
    return result;
}
SavePlacement installSaveRuntime(QByteArray &rom, const SaveRuntimeBuild &build,
                                 const SavePlacement &prior) {
    prior.verify(rom);
    auto next = build.placement;
    next.validate();
    if (!next.installed() || build.code.address != SavePlacement::ramAddress ||
        next.codeBytes != build.code.bytes.size() || next.codeChecksum != crc32(build.code.bytes))
        throw Error("Save module disagrees with its allocation");
    const auto catalog = saveRuntimeCatalog();
    if (!prior.installed()) {
        auto ranges = catalog["owners"].toArray();
        ranges.append(catalog["callbacks"]);
        for (const auto value : ranges) {
            const auto row = value.toObject();
            if (sha256(rom.mid(row["start"].toInt(), row["size"].toInt())) !=
                row["retailSha256"].toString().toLatin1())
                throw Error("Unrecognized save runtime owner at z64 " +
                            QString::number(row["start"].toInt(), 16));
        }
    }
    auto entry = [&](const QString &name) {
        if (!build.code.entries.contains(name))
            throw Error("Missing save entry " + name);
        return build.code.entries[name];
    };
    // The class linker has moved all class records before reclaiming their old
    // storage. Keep executable storage excluded from every later heap reset.
    rom.replace(SavePlacement::romAddress, next.codeBytes, build.code.bytes);
    for (const auto value : catalog["owners"].toArray()) {
        const auto row = value.toObject();
        const auto at = row["start"].toInt();
        put32(rom, at, 0x08000000U | ((entry(row["symbol"].toString()) >> 2) & 0x3ffffffU));
        put32(rom, at + 4, 0);
    }
    const auto table = catalog["callbacks"].toObject()["start"].toInt();
    put32(rom, table + 28 + 8, entry("lc_save_main_payload"));
    put32(rom, table + 4 * 28, entry("lc_discovery_init"));
    put32(rom, table + 4 * 28 + 4, entry("lc_discovery_load"));
    put32(rom, table + 4 * 28 + 8, entry("lc_discovery_save"));
    put32(rom, table + 4 * 28 + 12, 0x1844);
    next.ownerChecksum = saveRuntimeOwnerChecksum(rom);
    next.verify(rom);
    return next;
}
} // namespace lordly
