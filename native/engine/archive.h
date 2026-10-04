#pragma once
#include "engine/rom.h"

namespace lordly {
quint16 lhaCrc16(const QByteArray &bytes);
QByteArray decodeLh5(const QByteArray &compressed, qsizetype size);
struct LhaMember {
    qsizetype offset = 0, headerSize = 0, storedSize = 0, decodedSize = 0;
    QString name;
    int level = 0;
    bool compressed = false;
    quint16 checksum = 0;
    static LhaMember read(const QByteArray &rom, qsizetype offset);
    static LhaMember find(const Rom &rom, const QString &name);
    QByteArray decode(const Rom &rom) const;
};
} // namespace lordly
