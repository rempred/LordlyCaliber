#pragma once
#include "engine/custom_runtime.h"
#include "engine/custom_squads.h"
#include "rebuild/compiler.h"
namespace lordly {
struct CustomRuntimeBuild {
    CompiledModule code;
    QByteArray bytes;
    quint32 stateAddress = 0;
    quint32 indexRomOffset = 0;
    static constexpr quint32 stateSize = 144;
};
QMap<QString, quint32> customEncounterServices();
CustomRuntimeBuild buildCustomRuntime(const Compiler &compiler, const QString &directory,
                                      quint32 address, quint32 capacity, quint32 indexRomOffset,
                                      int poolSlots = 10, int profileCount = 0);
QByteArray rebuildCustomCallers(const Rom &rom, const Compiler &compiler, const QString &directory,
                                const QMap<QString, quint32> &entries,
                                const std::optional<CustomPlacement> &previous = std::nullopt);
QByteArray removeCustomInstallation(const Rom &rom, const CustomPlacement &previous,
                                    const Compiler &compiler, const QString &directory);
QByteArray installCustomEncounters(const Rom &rom, const CustomSquads &squads,
                                   const CustomRuntimeBuild &runtime, const Compiler &compiler,
                                   const QString &directory,
                                   const std::optional<CustomPlacement> &previous = std::nullopt,
                                   const QByteArray &poolPairs = {}, int poolSlots = 10);
} // namespace lordly
