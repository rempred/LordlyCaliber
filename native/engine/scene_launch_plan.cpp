#include "engine/scene_launch_plan.h"
namespace lordly {
bool SceneLaunchPlan::fresh(const DirectorProgram &program) {
    const int kind = program.resourceClass();
    if (kind == 2 || kind == 7 || kind == 6)
        return true;
    for (const auto &command : program.commands)
        if (command.opcode == 0x80000006U)
            return true;
    return false;
}
QVector<SceneLaunchPlan> SceneLaunchPlan::choices(const Directors &resources,
                                                  const SceneEvents &events, int selector) {
    auto program = [&](int id) -> const DirectorProgram & {
        if (id < 0 || id >= resources.selectors().size() ||
            !resources.entries().contains(resources.selectors()[id]))
            throw Error("The launch event selects an absent scene");
        return resources.entries().constFind(resources.selectors()[id]).value().program;
    };
    const auto &contexts = SceneEvents::contexts();
    auto findParent = [&](const SceneEventContext &child) -> const SceneEventContext * {
        const SceneEventContext *fallback = nullptr;
        for (const auto &candidate : contexts)
            if (candidate.site == child.parent) {
                if (candidate.invocation == child.parentInvocation)
                    return &candidate;
                if (!fallback)
                    fallback = &candidate;
            }
        return fallback;
    };
    std::function<QVector<SceneLaunchStep>(int, const SceneEventContext *, QSet<int>)> chain;
    chain = [&](int id, const SceneEventContext *context, QSet<int> seen) {
        if (seen.contains(id) || seen.size() >= 8)
            throw Error("Event predecessor chain is cyclic or exceeds eight scenes");
        seen.insert(id);
        SceneLaunchStep step;
        step.selector = id;
        if (context)
            step.translations = context->translations;
        QVector<SceneLaunchStep> steps;
        if (!fresh(program(id))) {
            if (!context || context->parent.isEmpty())
                throw Error("The event does not identify this scene's predecessor");
            steps = chain(events.target(context->parent), findParent(*context), seen);
            step.tick = steps.last().tick + context->ticks;
            const int parentKind = program(steps.first().selector).resourceClass();
            const int kind = program(id).resourceClass();
            if ((parentKind == 4 || parentKind == 5) != (kind == 4 || kind == 5))
                throw Error("Event predecessor changes the scene's rendering mode");
        }
        steps << step;
        return steps;
    };
    QVector<SceneLaunchPlan> result;
    for (const auto &context : contexts) {
        if (events.target(context.site) != selector)
            continue;
        try {
            const auto &site = SceneEvents::site(context.site);
            result << SceneLaunchPlan{context.id,
                                      QString("Event %1 · %2 · invocation %3")
                                          .arg(site.row)
                                          .arg(site.name)
                                          .arg(context.invocation),
                                      chain(selector, &context, {})};
        } catch (const Error &) {
            // An alternate invocation may have a usable predecessor; retain it.
        }
    }
    if (fresh(program(selector))) {
        if (result.isEmpty())
            result << SceneLaunchPlan{"direct", "Direct scene launch", {{selector}}};
        return result;
    }
    if (!result.isEmpty())
        return result;
    // Endings can be reached by a Director tail call instead of an event site.
    for (auto it = resources.entries().cbegin(); it != resources.entries().cend(); ++it) {
        if (it->selectors.isEmpty())
            continue;
        const int parent = it->selectors.first();
        if (parent == selector || !fresh(it->program))
            continue;
        for (int i = 0; i < it->program.commands.size(); ++i) {
            const auto &command = it->program.commands[i];
            if (command.opcode != 0x80000003U ||
                command.operands != QVector<quint32>{quint32(selector)})
                continue;
            SceneLaunchStep step;
            step.selector = parent;
            if (i > 0) {
                const auto &query = it->program.commands[i - 1];
                if (query.opcode == 0xbe && query.operands.size() == 2 && query.operands[0] == 0 &&
                    query.operands[1] < 3)
                    step.chaosBand = int(query.operands[1]);
            }
            result << SceneLaunchPlan{QString("caller:%1").arg(parent),
                                      QString("Caller scene %1").arg(parent),
                                      {step},
                                      selector};
        }
    }
    return result;
}
} // namespace lordly
