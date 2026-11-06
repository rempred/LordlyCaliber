#pragma once
#include "engine/scene_events.h"
#include "engine/story_routes.h"
#include <array>
namespace lordly {
enum class EventPhase { Arrival = 0, Mission = 1, AreaInvestigation = 2 };
struct EventProgramChoice {
    int row = -1, entry = -1;
    QString name;
};
struct EventLocationContext {
    int location = -1;
    std::array<QVector<EventProgramChoice>, 3> phases;
};
// Resolve navigation from the current event bytes. This does not evaluate a
// particular save's flags: all feasible world entry routes remain selectable.
class EventContexts {
  public:
    EventContexts(const Rom &, const SceneEvents &);
    EventLocationContext location(int) const;
    QVector<int> locationsForScenario(int key) const;
    QVector<int> locationsForProgram(int row, int offset = -1) const;
    QVector<int> scenarioPrograms(int key) const;
    QVector<int> scenarioKeys(int row) const;
    static QString programName(int row);
  private:
    QVector<int> destinations(int location, int routingRow) const;
    SceneEvents events_;
    StoryRoutes routes_;
    mutable QMap<int, EventLocationContext> locations_;
    mutable QMap<int, QVector<int>> keys_;
};
} // namespace lordly
