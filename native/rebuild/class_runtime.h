#pragma once
#include "engine/class_data.h"
#include "rebuild/compiler.h"

namespace lordly {
struct PromotionRuntimeBuild;
struct ClassRuntimeBuild {
    ClassDataImage data;
    QByteArray compiledData;
    CompiledModule code;
    ClassPlacement placement;
    QByteArray descriptor;
    quint32 dataRomOffset = 0;
    static constexpr quint32 codeRomOffset = 0x5db14;
    static constexpr quint32 directoryAddress = 0x80187e00;
    static constexpr int directoryCapacity = 10;
    static constexpr quint32 reclaimedStart = 0x80187f00, reclaimedEnd = 0x8018aa70;
};
// Compile the data and its initialization/reset functions. This does not install
// runtime references or produce a playable ROM on its own.
ClassRuntimeBuild buildClassRuntime(const ClassData &classes, const Compiler &compiler,
                                    const QString &directory, quint32 dataRomOffset);
QByteArray linkClassRuntime(const Rom &rom, const ClassRuntimeBuild &build,
                            const PromotionRuntimeBuild *menus = nullptr);
} // namespace lordly
