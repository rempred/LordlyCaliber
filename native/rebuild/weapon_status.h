#pragma once
#include "rebuild/compiler.h"

namespace lordly {
QByteArray compileStatusRecovery(const RuleBuild &, const Compiler &, const QString &directory);
void rebuildWeaponStatus(const Project &, const Compiler &, const QString &directory,
                         BuildResult &);
}
