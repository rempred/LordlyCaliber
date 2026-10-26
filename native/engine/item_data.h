#pragma once
#include "engine/rom.h"
#include "engine/weapon_types.h"
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
    const WeaponTypes &weaponTypes() const { return weaponTypes_; }
    void setWeaponTypes(const WeaponTypes &types) { weaponTypes_ = types; }
    qsizetype weaponTypesOffset() const;
    qsizetype loadedTypesOffset() const { return loadedTypesOffset_; }

  private:
    QByteArray prefix_;
    QVector<ItemAsset> entries_;
    WeaponTypes weaponTypes_;
    qsizetype loadedTypesOffset_ = 0;
};
} // namespace lordly
