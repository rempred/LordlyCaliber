#pragma once
#include "rebuild/compiler.h"
namespace lordly {
QByteArray rebuildPoseRuntime(const Rom &target, const Rom &source, const QByteArray &mappings,
                              bool wide, bool bodyPolicies, const Compiler &,
                              const QString &directory);
}
