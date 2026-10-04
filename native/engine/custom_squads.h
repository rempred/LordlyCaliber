#pragma once
#include "engine/class_data.h"
#include "engine/creature_rewards.h"
#include <QJsonArray>
#include <QMap>
#include <QVector>
namespace lordly {
struct CustomMember {
    int classId = 5, levelOffset = 0, cell = 8;
    std::array<int, 2> equipment{0, 0};
};
struct CustomSquad {
    int scenarioKey = 1, terrainSlot = 0;
    bool enabled = true, fixedPersuasion = true;
    QString label = "Bandits!";
    QVector<CustomMember> members{{5, 0, 8, {0, 0}}, {5, 0, 6, {0, 0}}};
    int chance = 10, retreatHp = 0;
    QVector<QPair<int, int>> bonuses;
    CreatureReward rewards;
    QJsonObject json() const;
    static CustomSquad parse(const QJsonObject &value, int classCount);
    QByteArray bytes() const;
};
class CustomSquads {
  public:
    static CustomSquads load(const Rom &rom, int classCount);
    const QMap<int, CustomSquad> &profiles() const {
        return profiles_;
    }
    static int key(int scenario, int slot);
    void set(const CustomSquad &value, int classCount);
    void remove(int scenario, int slot) {
        profiles_.remove(key(scenario, slot));
    }
    QJsonObject json() const;
    void apply(const QJsonObject &value, int classCount);
    void importLegacy(const QJsonObject &value, int classCount);
    void validateFormations(const ClassData &classes) const;
    QByteArray bytes(quint32 offset) const;
    int changedRows() const;
    bool modified() const {
        return json() != original_;
    }
    bool hasProfiles() const {
        return !profiles_.isEmpty();
    }

  private:
    QMap<int, CustomSquad> profiles_;
    QJsonObject original_{{"version", 1}, {"profiles", QJsonArray{}}};
};
} // namespace lordly
