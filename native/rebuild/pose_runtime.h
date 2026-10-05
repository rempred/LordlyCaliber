#pragma once
#include "rebuild/compiler.h"
namespace lordly {
QByteArray rebuildPoseRuntime(const Rom &target, const Rom &source, const QByteArray &mappings,
                              bool wide, const Compiler &, const QString &directory);
}
