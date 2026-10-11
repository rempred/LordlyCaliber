#pragma once
#include "engine/rom.h"
#include <QVector>
namespace lordly {
struct ItemAsset {
    QByteArray record, name;
};
// Equipment IDs use nine bits. 511 remains the class visual-policy sentinel.
class ItemData {
  public:
    static constexpr int retailCount = 278, maximumCount = 511;
    static constexpr quint32 retailBase = 0x8018c400;
    static ItemData load(const Rom &);
    const QVector<ItemAsset> &entries() const {
        return entries_;
    }
    int append(int source, const QByteArray &name);
    void set(int id, const ItemAsset &);
    QByteArray build(quint32 base) const;
    QByteArray recordsWithPrefix() const;

  private:
    QByteArray prefix_;
    QVector<ItemAsset> entries_;
};
} // namespace lordly
