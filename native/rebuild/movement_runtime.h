#pragma once
#include "engine/movement_runtime.h"
#include "engine/movement_types.h"
#include "rebuild/compiler.h"
namespace lordly {
struct MovementInput {
    MovementTypes types;
    Rom source;
    bool capacity = false;
    QMap<QString, quint32> bindings;
    int classCount = 0;
};
MovementPlacement buildMovementRuntime(const MovementInput &, quint32 address, quint32 offset,
                                       const Compiler &, const QString &,
                                       const std::function<quint32(quint32)> &relocate);
void installMovementRuntime(QByteArray &, const MovementPlacement &);
} // namespace lordly
