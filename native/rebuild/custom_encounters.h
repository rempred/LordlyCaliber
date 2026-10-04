#pragma once
#include "engine/custom_runtime.h"
#include "rebuild/compiler.h"
namespace lordly {
struct CustomRuntimeBuild {
    CompiledModule code;
    QByteArray bytes;
    quint32 stateAddress = 0;
    static constexpr quint32 stateSize = 144;
};
QMap<QString, quint32> customEncounterServices();
CustomRuntimeBuild buildCustomRuntime(const Compiler &compiler, const QString &directory,
                                      quint32 address, quint32 capacity, quint32 indexRomOffset);
QByteArray rebuildCustomCallers(const Rom &rom, const Compiler &compiler, const QString &directory,
                                const QMap<QString, quint32> &entries,
                                const std::optional<CustomPlacement> &previous = std::nullopt);
} // namespace lordly
