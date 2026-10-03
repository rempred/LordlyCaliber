#pragma once
#include "engine/class_data.h"

namespace lordly {
struct RuntimeDataReference {
    QString group;
    quint32 high = 0, low = 0;
    qint32 addend = 0;
};
struct RuntimeOwnerRecord {
    quint32 start = 0, size = 0;
    QByteArray canonicalSha256;
    QVector<RuntimeDataReference> references;
};
// ROM-side editor metadata; never included in the decompressed game resource.
// A complete installation must separately require every necessary runtime owner.
class ClassRuntimeInventory {
  public:
    int classCount = 0;
    QVector<RuntimeOwnerRecord> owners;
    static QMap<QString, quint32> canonicalBases();
    static QMap<QString, quint32> dataBases(const ClassDataLayout &layout);
    static RuntimeOwnerRecord capture(quint32 start, const QByteArray &code,
                                      const QVector<RuntimeDataReference> &references,
                                      const QMap<QString, quint32> &bases);
    QByteArray encode() const;
    static ClassRuntimeInventory decode(const QByteArray &bytes);
    QByteArray relocate(quint32 start, const QByteArray &code, const QMap<QString, quint32> &from,
                        const QMap<QString, quint32> &to) const;
    void verify(const Rom &rom, const QMap<QString, quint32> &bases) const;
};
} // namespace lordly
