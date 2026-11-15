#pragma once
#include "rebuild/compiler.h"
namespace lordly {
bool needsSceneLogic(const Project &);
bool hasSceneLogicRuntime(const Rom &);
CompiledModule buildSceneLogicRuntime(const Compiler &,const QString &,quint32 address,quint32 get,quint32 set);
void installSceneLogicRuntime(QByteArray &,const Rom &source,quint32 entry);
}
