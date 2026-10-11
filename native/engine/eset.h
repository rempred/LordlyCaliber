#pragma once
#include "engine/rom.h"
#include <QVector>

namespace lordly {
// ESET framing only. Gameplay/editing constraints belong to the scenario model.
// Unnamed fields and trailing data survive an unchanged decode/encode.
struct Eset {
    QByteArray header, seed, tail;
    QVector<QByteArray> deployments, nodes, conditions;
    bool conditionsPresent = false;
    static Eset decode(const QByteArray &data);
    QByteArray encode() const;
    int baseLevel() const;
    void setBaseLevel(int level);
};
} // namespace lordly
