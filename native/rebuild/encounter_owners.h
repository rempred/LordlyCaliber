#pragma once
#include "rebuild/compiler.h"
namespace lordly {
QByteArray compileEncounterOwner(const Compiler &, const QString &kind,
                                 const QByteArray &definitions, const QString &directory,
                                 const QString &name, const QMap<QString, quint32> &entries = {});
}
