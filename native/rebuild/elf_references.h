#pragma once
#include "rebuild/compiler.h"

namespace lordly {
QVector<CompiledAddressReference> readMipsAddressReferences(const QByteArray &object,
                                                            const CompiledModule &linked,
                                                            const QMap<QString, quint32> &symbols);
} // namespace lordly
