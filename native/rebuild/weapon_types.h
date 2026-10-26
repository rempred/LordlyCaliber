#pragma once
#include "rebuild/compiler.h"
namespace lordly {
QByteArray removeWeaponTypes(const Rom &, const Compiler &, const QString &directory);
void rebuildWeaponTypes(const Project &, const Compiler &, const QString &directory, BuildResult &);
} // namespace lordly
