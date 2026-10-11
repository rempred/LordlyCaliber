#pragma once
#include "engine/world_resources.h"
#include <QJsonObject>
#include <QSet>

namespace lordly {
struct WorldScenario {
    QByteArray treasures, sites;
    bool operator==(const WorldScenario &) const = default;
};
struct WorldPlan {
    QByteArray towns;
    QMap<int, QByteArray> treasures, sites;
};
class ScenarioWorld {
  public:
    static constexpr int townCount = 316, treasureCapacity = 16;
    static ScenarioWorld load(const Rom &rom);
    const WorldScenario &scenario(int key) const;
    const WorldScenario &original(int key) const;
    QByteArray town(int id) const;
    QString townName(int id) const;
    void setTown(int id, const QByteArray &record);
    void setSiteAllegiance(int key, int row, int allegiance); // 0 neutral, 1 allied, 2 enemy.
    void setTreasure(int key, int row, const QByteArray &record);
    int addTreasure(int key);
    void removeTreasure(int key, int row);
    void restoreScenario(int key);
    void restoreTown(int id);
    QJsonObject json() const;
    void apply(const QJsonObject &value);
    void importBrowser(const Rom &rom, const QJsonObject &value);
    WorldPlan plan() const;
    bool modified() const;
    int changedEntries() const;
    int siteCount(int key) const;
    static int declaredSiteCount(const QByteArray &data);
    static int treasureCount(const QByteArray &data);
    static QByteArray treasureRecord(const QByteArray &data, int row);

  private:
    void validateTown(int id, const QByteArray &record) const;
    void validateScenario(int key, const WorldScenario &value) const;
    QMap<int, WorldScenario> rows_, originals_;
    QByteArray towns_, originalTowns_;
    QSet<int> reservedTreasureIds_;
    QMap<int, int> originalTreasureGroups_;
    QMap<int, int> siteCounts_;
};
} // namespace lordly
