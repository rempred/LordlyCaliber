#pragma once
#include "engine/animation_overrides.h"
#include "rebuild/compiler.h"
namespace lordly {
QByteArray rebuildAnimationOverrides(const Rom &, const AnimationOverrides &, const ClassData &,
                                     const Compiler &, const QString &directory);
}
