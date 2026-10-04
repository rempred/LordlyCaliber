#pragma once
#include "engine/archive.h"
#include <QJsonObject>
#include <QStringList>
#include <QVector>

namespace lordly {
struct ShopStock {
    QVector<quint16> equipment, consumables;
    bool operator==(const ShopStock &) const = default;
};
class Shops {
  public:
    static constexpr int count = 36, equipmentCapacity = 50, consumableCapacity = 30;
    static Shops load(const Rom &rom);
    const ShopStock &stock(int id) const;
    const ShopStock &originalStock(int id) const;
    const QStringList &locations(int id) const;
    void setStock(int id, const ShopStock &stock);
    bool modified() const;
    int changedShops() const;
    QJsonObject json() const;
    void apply(const QJsonObject &object);

  private:
    static void validate(int id, const ShopStock &stock);
    QVector<ShopStock> original_, stock_;
    QVector<QStringList> locations_;
};
} // namespace lordly
