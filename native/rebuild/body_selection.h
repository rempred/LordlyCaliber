#pragma once
#include "rebuild/compiler.h"
namespace lordly {
QByteArray rebuildBodySelection(const Rom &target, const Rom &source, const QByteArray &mappings,
                                const Compiler &, const QString &directory);
}
