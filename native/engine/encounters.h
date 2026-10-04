#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <QMap>
#include <QStringList>
#include <QVector>
namespace lordly {
class Encounters {
  public:
    static constexpr int poolCount = 45, slotCount = 10, terrainCount = 32;
    static constexpr int poolOffset = 0x141ed4, rateOffset = 0x141e80, slotOffset = 0x141ea0;
    static Encounters load(const Rom &rom, int classCount);
    static void verifyReader(const Rom &rom);
    static QStringList terrainNames();
    int classAt(int pool, int slot, int side) const;
    int rate(int terrain) const;
    int lookup(int terrain) const;
    QVector<int> scenarioKeys(int pool) const;
    QString label(int pool) const;
    void setPair(int pool, int slot, int first, int second, int classCount);
    void setTerrain(int terrain, int rate, int lookup);
    const QByteArray &poolBytes() const {
        return pools_;
    }
    const QByteArray &rateBytes() const {
        return rates_;
    }
    const QByteArray &slotBytes() const {
        return slots_;
    }
    bool modified() const;
    int changedRows() const;
    QJsonObject json() const;
    void apply(const QJsonObject &object, int classCount);
    void importLegacy(const QJsonObject &object, int classCount);

  private:
    static int pairOffset(int pool, int slot);
    static void terrainIndex(int terrain);
    QByteArray pools_, rates_, slots_, originalPools_, originalRates_, originalSlots_;
    QVector<QVector<int>> keys_;
    QMap<int, QString> names_;
};
} // namespace lordly
