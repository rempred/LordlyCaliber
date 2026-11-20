#include "engine/scene_launch_plan.h"
#include "engine/event_script.h"
#include <algorithm>
namespace lordly {
namespace {
bool usesRewardPanel(const DirectorProgram &program) {
    return std::any_of(program.commands.cbegin(), program.commands.cend(), [](const auto &command) {
        return command.opcode == 0x62 && command.operands.size() == 2 && command.operands[1] == 34;
    });
}
DirectorLaunchTranslations launchArguments(const SceneEventContext &context,
                                          const DirectorProgram &program, const SceneEvents &events) {
    auto arguments = context.translations;
    if (usesRewardPanel(program)) {
        const auto &site = SceneEvents::site(context.site);
        arguments.known[255] = 0;
        if (const auto reward = EventScript::launchReward(events.program(site.row), context.invocation, site.offset))
            arguments.set(255, *reward);
    }
    return arguments;
}
SceneGift eventGift(const SceneEvents &events, const SceneEventSite &site, const DirectorLaunchTranslations &arguments) {
    SceneGift gift;
    if (arguments.known[255]) gift.shown = arguments.values[255];
    const auto effects = EventScript::giftEffects(events.program(site.row), site.offset);
    gift.grants = effects.consumableGrants;
    for (const auto &[id, set] : effects.flags)
        gift.flags << (set ? "Set " : "Clear ") + events.flagName(id,site.row);
    return gift;
}
}
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
            step.translations = launchArguments(*context, program(id), events);
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
                                      chain(selector, &context, {}), -1, context.site};
            if (usesRewardPanel(program(selector)))
                result.last().gift = eventGift(events, site, result.last().steps.last().translations);
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
void SceneLaunchPlan::addAnswerRoutes(QVector<SceneLaunchPlan> &plans, const Rom &rom,
                                      const ScenePrograms &scenes, const Directors &directors, const SceneEvents &events,
                                      int selector, const QMap<int,bool> &sampleFlags) {
    struct Row {
        QByteArray bytes;
        QVector<StoryEvent> flow;
        QJsonObject gates;
    };
    QMap<int, Row> rows;
    const auto &contexts = SceneEvents::contexts();
    for (auto &plan : plans) {
        plan.decision.reset();
        if (plan.site.isEmpty())
            continue;
        const auto &site = SceneEvents::site(plan.site);
        const auto parent = std::find_if(contexts.cbegin(), contexts.cend(),
                                         [&](const auto &c) { return c.id == plan.id; });
        if (parent == contexts.cend())
            continue;
        // The reaction inherits the exact launch arguments of this event invocation.
        auto child = [&](const SceneEventContext &c) {
            return c.parent == plan.site && c.parentInvocation == parent->invocation;
        };
        if (std::none_of(contexts.cbegin(), contexts.cend(), child))
            continue;
        if (!rows.contains(site.row))
            rows.insert(
                site.row,
                {events.program(site.row), EventFlow::read(rom, events, scenes, site.row, false),
                 events.flagEdits()[QString::number(site.row)].toObject()["gates"].toObject()});
        const auto &row = rows.constFind(site.row).value();
        auto decision = EventFlow::sceneDecision(row.flow, row.bytes, site, selector, row.gates, sampleFlags);
        if (!decision)
            continue;
        auto arguments = [&](SceneAnswerReaction &arm) {
            if (arm.selector < 0)
                return false;
            const auto executed = arm.translations;
            bool found = false;
            for (const auto &c : contexts)
                if (child(c)) {
                    const auto &target = SceneEvents::site(c.site);
                    if (target.row != site.row || target.offset != arm.requestOffset ||
                        events.target(c.site) != arm.selector)
                        continue;
                    const auto key = directors.selectors().value(arm.selector);
                    const auto resource = directors.entries().constFind(key);
                    if (resource == directors.entries().cend())
                        return false;
                    auto translated = launchArguments(c, resource->program, events);
                    for (int i = 0; i < int(executed.known.size()); ++i)
                        if (executed.known[i])
                            translated.set(i, executed.values[i]);
                    if (found && (arm.translations.known != translated.known ||
                                  arm.translations.values != translated.values))
                        return false;
                    arm.translations = translated;
                    if (usesRewardPanel(resource->program)) arm.gift = eventGift(events, target, translated);
                    found = true;
                }
            return found;
        };
        if (!arguments(decision->first))
            decision->first = {};
        if (!arguments(decision->second))
            decision->second = {};
        if (decision->first.selector >= 0 || decision->second.selector >= 0 || !decision->missingFlags.isEmpty())
            plan.decision = std::move(decision);
    }
}
} // namespace lordly
