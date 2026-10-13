#pragma once
#include "engine/save_runtime.h"
#include "rebuild/compiler.h"
namespace lordly {
struct SaveRuntimeBuild {
    CompiledModule code;
    CompiledModule compression;
    CompiledModule capacityMigration;
    SavePlacement placement;
    QMap<quint32, QByteArray> pakFragments;
};
SaveRuntimeBuild buildSaveRuntime(quint32 discovery, const Compiler &compiler,
                                  const QString &directory, quint32 compressionAddress,
                                  bool expandedArmies = false);
SavePlacement installSaveRuntime(QByteArray &rom, const SaveRuntimeBuild &build,
                                 const SavePlacement &prior);
} // namespace lordly
