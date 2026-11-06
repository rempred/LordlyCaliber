#pragma once
#include "rebuild/compiler.h"
namespace lordly {
CompiledModule buildRetailEventGateRuntime(const Compiler &,const QString &directory,quint32 address);
// The source ROM authenticates the previous dispatch target before a rebuild
// moves or removes its resident allocation. A zero bridge restores the no-op.
void linkRetailEventGateRuntime(QByteArray &image,quint32 bridgeAddress,const Rom &source);
}
