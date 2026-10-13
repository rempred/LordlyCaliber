#include "rebuild/save_runtime.h"
#include <QFile>
#include <QJsonArray>
namespace lordly {
SaveRuntimeBuild buildSaveRuntime(quint32 discovery, const Compiler &compiler,
                                  const QString &directory, quint32 compressionAddress,
                                  bool expandedArmies) {
    auto source = [](const QString &name) {
        QFile file(":/game/classes/" + name);
        if (!file.open(QIODevice::ReadOnly))
            throw Error("Missing maintained save source " + name);
        return file.readAll();
    };
    SaveRuntimeBuild result;
    result.compression = compiler.compileModule(source("save-compression.c"), directory,
                                                "save_compression", compressionAddress,
                                                {{"save_compression.h", source("save_compression.h")}}, {});
    QMap<QString, quint32> services = {{"lc_save_cache", 0x800a83b8},
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
                                          {"lc_native_options_save", 0x8016cdf4},
                                          {"lc_native_clear", 0x80093380}};
    for (const auto &name : {QString("lc_save_frame_encode"), QString("lc_save_frame_decode"),
                             QString("lc_save_data_crc")}) {
        if (!result.compression.entries.contains(name)) throw Error("Missing save compression service");
        services.insert(name, result.compression.entries.value(name));
    }
    services.insert("lc_native_invalidate_code", 0x800900c0);
    services.insert("lc_native_class_dma", 0x8009da50);
    const QByteArray prepareCode = "\nvoid lc_native_invalidate_code(void *, unsigned int);\n"
        "void lc_native_class_dma(unsigned int, void *, unsigned int);\n"
        "void lc_save_load_data(unsigned int rom, void *data, unsigned int size) {\n"
        "    lc_native_class_dma(rom, data, size);\n"
        "    lc_native_invalidate_code(data, size);\n}\n";
    QByteArray capacityCode;
    QMap<QString,QByteArray> headers{{"save_format.h",source("save_format.h")},
        {"save_compression.h",source("save_compression.h")},{"save_checksums.h",source("save_checksums.h")}};
    if(expandedArmies) {
        QFile migration(":/game/scenarios/capacity-save.c"),header(":/game/scenarios/capacity-save.h");
        if(!migration.open(QIODevice::ReadOnly)||!header.open(QIODevice::ReadOnly))
            throw Error("Missing expanded-army save migration");
        capacityCode="\n"+migration.readAll();headers["capacity-save.h"]=header.readAll();
        services["lc_save_expand_capacity"]=(compressionAddress+quint32(result.compression.bytes.size())+15U)&~15U;
    }
    const auto runtimeSource=(expandedArmies?QByteArray("#define LC_SAVE_RUNTIME_FORMAT 3\n"):QByteArray{})+
        source("save-format.c")+"\n"+source("save-image.c")+"\n"+source("save-runtime.c")+prepareCode;
    result.code=compiler.compileModule(runtimeSource,directory,"save_runtime",SavePlacement::ramAddress,headers,services);
    if(expandedArmies) {
        result.capacityMigration=compiler.compileModule(capacityCode,directory,"capacity_migration",
            services["lc_save_expand_capacity"],headers,result.code.entries);
        services["lc_save_expand_capacity"]=result.capacityMigration.entries.value("lc_save_expand_capacity");
        const auto rebound=compiler.compileModule(runtimeSource,directory,"save_runtime",SavePlacement::ramAddress,headers,services);
        if(!services["lc_save_expand_capacity"] || rebound.entries!=result.code.entries || rebound.bytes.size()!=result.code.bytes.size())
            throw Error("Linking the save migration changed resident service placement");
        result.code=rebound;
    }
    result.placement.codeBytes = quint32(result.code.bytes.size());
    result.placement.codeChecksum = crc32(result.code.bytes);
    result.placement.version = 4;
    result.placement.validate();
    for (const auto value : saveRuntimeCatalog()["pakOwners"].toArray()) {
        const auto row = value.toObject();
        const auto symbol = row["symbol"].toString();
        const auto delta = quint32(row["delta"].toDouble());
        const auto bytes =
            compiler.assembleOwner(source(row["source"].toString()), directory, symbol,
                                   delta + quint32(row["fragment"].toInt()),
                                   {{symbol, result.code.entries.value(symbol)},
                                    {symbol + "_continue", delta + quint32(row["resume"].toInt())},
                                    {symbol + "_reject", delta + quint32(row["reject"].toInt())}});
        if (!result.code.entries.contains(symbol) || bytes.isEmpty() ||
            bytes.size() > row["fragmentSize"].toInt())
            throw Error("Controller Pak service exceeds its source allocation");
        result.pakFragments[row["fragment"].toInt()] =
            bytes + QByteArray(row["fragmentSize"].toInt() - bytes.size(), 0);
    }
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
    if (prior.version < 2)
        for (const auto value : catalog["pakOwners"].toArray()) {
            const auto row = value.toObject();
            if (sha256(rom.mid(row["start"].toInt(), row["size"].toInt())) !=
                row["retailSha256"].toString().toLatin1())
                throw Error("Unrecognized Controller Pak runtime owner");
        }
    if (prior.version < 3)
        for (const auto value : catalog["lifecycleOwners"].toArray()) {
            const auto row = value.toObject();
            if (sha256(rom.mid(row["start"].toInt(), row["size"].toInt())) !=
                row["retailSha256"].toString().toLatin1())
                throw Error("Unrecognized new-game reset owner");
        }
    if (prior.version < 4)
        for (const auto value : catalog["suspendMenuOwners"].toArray()) {
            const auto row = value.toObject();
            if (sha256(rom.mid(row["start"].toInt(), row["size"].toInt())) !=
                row["retailSha256"].toString().toLatin1())
                throw Error("Unrecognized suspend menu owner");
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
    for (const auto value : catalog["pakOwners"].toArray()) {
        const auto row = value.toObject();
        const auto start = row["fragment"].toInt(), size = row["fragmentSize"].toInt();
        if (!build.pakFragments.contains(start) || build.pakFragments[start].size() != size)
            throw Error("Missing Controller Pak rebuild fragment");
        rom.replace(start, size, build.pakFragments[start]);
    }
    for (const auto value : catalog["lifecycleOwners"].toArray()) {
        const auto row = value.toObject();
        put32(rom, row["call"].toInt(),
              0x0c000000U | ((entry(row["symbol"].toString()) >> 2) & 0x3ffffffU));
    }
    for (const auto value : catalog["suspendMenuOwners"].toArray()) {
        const auto row = value.toObject();
        put32(rom, row["call"].toInt(), 0x0c000000U | ((entry(row["symbol"].toString()) >> 2) & 0x3ffffffU));
        put32(rom, row["resultDelay"].toInt(), 0);
    }
    next.ownerChecksum = saveRuntimeOwnerChecksum(rom, next.version);
    next.verify(rom);
    return next;
}
} // namespace lordly
