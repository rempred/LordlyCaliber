#pragma once
#include "rebuild/compiler.h"
namespace lordly {
QByteArray rebuildBodyMarkers(const Rom &target, const Rom &source, bool wide, const Compiler &,
                              const QString &directory);
}
