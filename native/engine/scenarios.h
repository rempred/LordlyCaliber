#pragma once
#include "engine/class_data.h"
#include "engine/eset.h"
#include "engine/scenario_resources.h"

namespace lordly {
struct Scenario {
    int key = 0;
    QString name;
    Eset layout;
    QVector<QByteArray> squads;
    QJsonObject json() const;
};
struct ScenarioPlan {
    QByteArray enemies;
    QMap<int, QByteArray> esets;
};
enum class SquadBehavior { Guard, Intercept, Move, MoveAndAttack, HitAndRun, Evade, Retreat };
struct SquadCommand {
    SquadBehavior behavior = SquadBehavior::Guard;
    int x = 128, z = 128; // A zero Z selects the one-based scenario point in X.
};
class Scenarios {
  public:
    static Scenarios load(const Rom &rom);
    const QMap<int, Scenario> &entries() const {
        return rows_;
    }
    const Scenario &scenario(int key) const;
    const Scenario &original(int key) const;
    void setBaseLevel(int key, int level, const ClassData &classes);
    void setSquad(int key, int row, const QByteArray &record, const ClassData &classes);
    void setDeployment(int key, int row, const QByteArray &record, const ClassData &classes);
    void editSquad(int key, int row, const QByteArray &squad, const QByteArray &deployment,
                   const ClassData &classes);
    int addSquad(int key, int templateRow, const ClassData &classes);
    void removeSquad(int key, int row, const ClassData &classes);
    void setNode(int key, int row, const QByteArray &record, const ClassData &classes);
    int addNode(int key, int kind, const ClassData &classes);
    void removeNode(int key, int row, const ClassData &classes);
    void setCondition(int key, int row, const QByteArray &record, const ClassData &classes);
    int addCondition(int key, int kind, const ClassData &classes);
    void removeCondition(int key, int row, const ClassData &classes);
    void setRouteGroup(int key, int squadRow, int group, int pattern, const ClassData &classes);
    std::optional<SquadCommand> squadCommand(int key, int squadRow) const;
    void setSquadCommand(int key, int squadRow, const SquadCommand &, const ClassData &classes);
    void restore(int key);
    void apply(const QJsonObject &value, const ClassData &classes);
    void importBrowser(const Rom &rom, const QJsonObject &scenarioProject,
                       const QJsonObject &overrides, const ClassData &classes);
    QJsonObject json() const;
    ScenarioPlan plan(const ClassData &classes) const;
    void validateForExport(const ClassData &classes) const;
    int changedScenarios() const;
    bool modified() const {
        return changedScenarios() != 0;
    }
    static int memberCount(const QByteArray &record);
    static int carryingCapacity(const QByteArray &record, const ClassData &classes);
    static bool automaticAlly(int key, int oneBasedTemplate);

  private:
    void replace(Scenario value, const ClassData &classes);
    void validate(const Scenario &value, const ClassData &classes) const;
    void validateRoutes(const Scenario &value) const;
    QMap<int, Scenario> rows_, originals_;
    QByteArray enemies_;
    QVector<QPair<int, int>> originalClassLimits_;
};
} // namespace lordly
