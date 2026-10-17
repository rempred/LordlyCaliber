#pragma once
#include "engine/persuasion.h"
#include "rebuild/compiler.h"
namespace lordly {
QByteArray rebuildPersuasion(const Rom &, const Persuasion &, const Compiler &, const QString &directory);
}
