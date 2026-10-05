#pragma once
#include "engine/rom.h"
#include <QMap>
#include <QVector>
#include <array>
namespace lordly {
// The twelve-bit direct-handle directory, shared by combat bodies and Stage art.
// Descriptor payloads have separate owners; this owner rebuilds their directory.
class DirectDescriptors {
  public:
    static constexpr quint32 resourceBase = 0x594280, retailKey = 0x3b6cd0;
    static constexpr int descriptorOffset = 0x27c4d00, descriptorSize = 96;
    static const std::array<QPair<int, int>, 12> &sites();
    static DirectDescriptors load(const Rom &);
    static quint32 resourceKey(const Rom &);
    static QByteArray directory(const Rom &);
    static QByteArray canonicalOwner(QByteArray, qsizetype start);
    static void validateRelocations(const Rom &);
    const QVector<quint32> &keys() const {
        return keys_;
    }
    // 1-based descriptor IDs. High handle bits belong to the image variant.
    int append(quint32 descriptorKey);
    void set(int id, quint32 descriptorKey);
    QByteArray decoded() const;
    QByteArray rebuild(const Rom &, const QByteArray &compiled) const;

  private:
    QByteArray source_, descriptor_;
    QVector<quint32> keys_;
    QMap<int, quint32> words_;
    quint32 offset_ = 0, size_ = 0;
};
} // namespace lordly
