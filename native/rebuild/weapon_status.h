#pragma once
#include "rebuild/compiler.h"

namespace lordly {
void rebuildWeaponStatus(const Project &, const Compiler &, const QString &directory,
                         BuildResult &);
}
