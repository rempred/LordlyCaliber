#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <QVector>

namespace lordly {
// Native field resources for independently editable, newly authored missions.
class BattlefieldClone {
  public:
    static constexpr quint32 descriptorOffset = 0x27c5500, descriptorSize = 64;
    static bool supportsSource(const Rom &,int source);
    static QJsonObject capture(const Rom &, int source, bool blank,
                               const QVector<int> &pointRows = {});
    static QByteArray apply(const Rom &, int target, const QJsonObject &);
    static void validate(const Rom &, int target, const QJsonObject &);
    static quint32 resourceKey(const Rom &, quint32 original);
    static QByteArray modelTable(const Rom &);
    static QByteArray modelRow(const Rom &, int model);
    static int modelCount(const Rom &);
    static void replaceCollection(QByteArray &,quint32 original,const QByteArray &);
    // The table is loaded by the ordinary bounded resident extension allocator.
    // Run after accepted C is emitted. This only rebinds native resource-key
    // literals and model-data load displacements; no native control flow or
    // behavioral algorithm is replaced. Opcode/register qualification is exact.
    static void linkRuntime(QByteArray &, quint32 tableAddress);
};
}
