#pragma once
#include "engine/consumable_effects.h"
#include "engine/consumable_tree_runtime.h"
#include "rebuild/compiler.h"
namespace lordly {
struct ConsumableTreeInput { ConsumableEffects effects; Rom source; };
ConsumableTreePlacement buildConsumableTrees(const ConsumableTreeInput &, quint32 address,
    quint32 offset, const Compiler &, const QString &, const std::function<quint32(quint32)> &relocate);
QByteArray installConsumableTrees(const QByteArray &image,const ConsumableTreePlacement &);
}
