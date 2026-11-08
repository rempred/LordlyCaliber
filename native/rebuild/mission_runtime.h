#pragma once
#include "rebuild/compiler.h"
namespace lordly {
struct MissionRuntimeBuild {
    QByteArray bytes;
    QMap<QString, quint32> symbols;
    bool hasBattlefields = false;
};
MissionRuntimeBuild buildMissionRuntime(const Project &, const Compiler &, const QString &directory,
                                       quint32 address, quint32 capacity);
QByteArray linkMissionRuntime(const Rom &, const Project &, const MissionRuntimeBuild &,
                             const Compiler &, const QString &directory);
}
