#pragma once
#include "rebuild/compiler.h"
namespace lordly {
QByteArray rebuildBodyVisualPolicy(const Rom &, const Rom &, const AnimationRoutes &,
                                   const Compiler &, const QString &);
}
