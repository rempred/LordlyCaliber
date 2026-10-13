#pragma once
#include "engine/class_data.h"
#include "rebuild/compiler.h"

namespace lordly {
// All arrays have independent allocations. Authored map sources end at59;
// slot60 is reserved for temporary wild encounters and never borrows a player.
struct CapacityRuntimeBuild {
    quint32 base = 0, codeAddress = 0;
    QByteArray storage, code;
    QMap<QString, quint32> symbols, entries, custom;
    QMap<quint32, quint32> words, expected;
    QMap<quint32, quint32> rewrittenOwners;
    QMap<quint32, quint32> entryGuards;
    QMap<quint32, QByteArray> ownerGuards;
    QByteArray trainingVariant;
    quint32 relocated(quint32 address, const ClassDataLayout &classes) const;
};
CapacityRuntimeBuild prepareCapacityRuntime(quint32 base, const Rom &rom);
void compileCapacityRuntime(CapacityRuntimeBuild &build, quint32 codeAddress,
                            const ClassDataLayout &classes, const Compiler &compiler,
                            const QString &directory);
// Applies only exact guarded instruction/data operands. Kept separate from
// export orchestration so tests can inspect every allocation and final target.
void applyCapacityRuntime(QByteArray &rom, const CapacityRuntimeBuild &build);
QByteArray installCapacityRuntime(const Rom &rom,const CapacityRuntimeBuild &build);
} // namespace lordly
