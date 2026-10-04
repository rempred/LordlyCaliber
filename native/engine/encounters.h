#pragma once
#include "engine/creature_rewards.h"
#include "engine/custom_squads.h"
#include "engine/encounter_rate.h"
#include "engine/rom.h"
#include "engine/scenario_rates.h"
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
    QString scenarioLabel(int key) const {
        return names_.value(key, QString("Scenario key %1").arg(key));
    }
    void setPair(int pool, int slot, int first, int second, int classCount);
    void setTerrain(int terrain, int rate, int lookup);
    const EncounterRate &globalRate() const {
        return globalRate_;
    }
    void setGlobalRate(const QJsonObject &value) {
        globalRate_.apply(value);
    }
    void importLegacyGlobalRate(const QJsonObject &value) {
        globalRate_.importLegacy(value);
    }
    const CreatureRewards &rewards() const {
        return rewards_;
    }
    void setReward(int id, const CreatureReward &value) {
        rewards_.setRow(id, value);
    }
    void importLegacyRewards(const QJsonObject &value) {
        rewards_.importLegacy(value);
    }
    const ScenarioRates &scenarioRates() const {
        return scenarioRates_;
    }
    void setScenarioBranch(int key, bool normal, const std::optional<ScenarioRoll> &value) {
        scenarioRates_.setBranch(key, normal, value);
    }
    void importLegacyScenarioRates(const QJsonObject &value) {
        scenarioRates_.importLegacy(value);
    }
    const CustomSquads &customSquads() const {
        return customSquads_;
    }
    void setCustomSquad(const CustomSquad &value, int classCount) {
        customSquads_.set(value, classCount);
    }
    void removeCustomSquad(int key, int slot) {
        customSquads_.remove(key, slot);
    }
    void importLegacyCustomSquads(const QJsonObject &value, int classCount) {
        customSquads_.importLegacy(value, classCount);
    }
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
    EncounterRate globalRate_;
    CreatureRewards rewards_;
    ScenarioRates scenarioRates_;
    CustomSquads customSquads_;
};
} // namespace lordly
