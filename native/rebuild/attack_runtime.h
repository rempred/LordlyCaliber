#pragma once
#include "engine/attack_data.h"
#include "rebuild/compiler.h"
namespace lordly {
QByteArray compileAttackRuntime(const AttackData &, AttackPlacement &, const Compiler &,
                                const QString &directory,
                                const std::function<quint32(quint32)> &relocateNative = {});
// Restore only authenticated attack-owned words before other feature exporters.
QByteArray removeAttackReferences(const Rom &);
QByteArray installAttackReferences(const Rom &, const AttackPlacement &);
} // namespace lordly
