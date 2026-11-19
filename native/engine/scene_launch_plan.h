#pragma once
#include "engine/event_flow.h"
namespace lordly {
struct SceneLaunchStep {
    int selector = -1, tick = 0;
    DirectorLaunchTranslations translations;
    std::optional<int> chaosBand;
};
struct SceneLaunchPlan {
    QString id, name;
    QVector<SceneLaunchStep> steps;
    // Tail calls enter through the parent's real replacement command, rather
    // than starting a second simultaneous Director.
    int tailCall = -1;
    QString site;
    std::optional<SceneAnswerDecision> decision;
    static bool fresh(const DirectorProgram &);
    static QVector<SceneLaunchPlan> choices(const Directors &, const SceneEvents &, int selector);
    static void addAnswerRoutes(QVector<SceneLaunchPlan> &, const Rom &, const ScenePrograms &,
                                const Directors &, const SceneEvents &, int selector);
};
} // namespace lordly
