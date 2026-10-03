#include "rebuild/class_runtime.h"
#include <QFile>

namespace lordly {
ClassRuntimeBuild buildClassRuntime(const ClassData &classes, const Compiler &compiler,
                                    const QString &directory, quint32 dataRomOffset) {
    ClassRuntimeBuild result;
    result.data = classes.build(0x80243db0);
    result.dataRomOffset = dataRomOffset;
    if ((dataRomOffset & 15) || dataRomOffset < 0x1000 ||
        quint64(dataRomOffset) + result.data.bytes.size() > 0x4000000)
        throw Error("Class data needs an aligned allocation within the cartridge address range");
    result.compiledData = compiler.compileData(result.data.bytes, directory, "class_data");
    QFile source(":/game/classes/class-runtime.c");
    if (!source.open(QIODevice::ReadOnly))
        throw Error("Missing maintained class initialization source");
    const auto header =
        QString("#define LC_RESERVED_BYTES %1\n"
                "#define LC_RECLAIM_START %2U\n"
                "#define LC_RECLAIM_SIZE %3\n"
                "#define LC_ROM_DATA %4U\n")
            .arg(result.data.bytes.size())
            .arg(ClassRuntimeBuild::reclaimedStart)
            .arg(ClassRuntimeBuild::reclaimedEnd - ClassRuntimeBuild::reclaimedStart)
            .arg(dataRomOffset)
            .toLatin1();
    result.code = compiler.compileModule(source.readAll(), directory, "class_runtime", 0x80187c14,
                                         {{"class_runtime_config.h", header}},
                                         {{"lc_original_heap_init", 0x80070c60},
                                          {"lc_original_pool_add", 0x80070d20},
                                          {"lc_original_secondary_reset", 0x80070f14},
                                          {"lc_dma_read", 0x8009da50}});
    if (quint64(result.code.address) + result.code.bytes.size() >
            ClassRuntimeBuild::directoryAddress ||
        result.code.entries.size() != 3 || !result.code.entries.contains("lc_heap_reset") ||
        !result.code.entries.contains("lc_secondary_reset") ||
        !result.code.entries.contains("lc_boot_classes"))
        throw Error("Class initialization module exceeds or disagrees with its runtime layout");
    return result;
}
} // namespace lordly
