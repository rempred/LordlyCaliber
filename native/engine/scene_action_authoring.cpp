#include "engine/scene_action_control.h"
#include "engine/scene_actions.h"
#include <algorithm>
#include <optional>
namespace lordly {
namespace action_detail {
namespace {
bool uncertainControl(const DirectorCommand &c) {
    return (Directors::grammar()[c.opcode].queryWords && c.opcode != 0xe && c.opcode != 0xb5) ||
           c.opcode == 0x56 || c.opcode == 0x59 || c.opcode == 0x99 || c.opcode == 0x9a ||
           c.opcode == 0xbb || c.opcode == 0x39 || c.opcode == 0x80000003 || c.opcode == 0x80000005;
}
} // namespace
bool completeDialogue(const DirectorProgram &p, int index) {
    const auto &c = p.commands;
    if (index < 0 || index + 6 >= c.size() || c[index].opcode != 0xbf)
        return false;
    const auto id = c[index].operands[0];
    return id <= 255 && c[index + 1].opcode == 0 && c[index + 2].opcode == 0x80000002 &&
           c[index + 3].opcode == 0x80000000 && c[index + 4].opcode == 0x10 &&
           c[index + 4].operands == QVector<quint32>{0, 2, id} && c[index + 5].opcode == 6 &&
           c[index + 5].operands == QVector<quint32>{id} && c[index + 6].opcode == 5 &&
           c[index + 6].operands == QVector<quint32>{id};
}
QVector<CounterState> counterStates(const DirectorProgram &program) {
    QVector<CounterState> result;
    CounterState state;
    int queriesSinceYield = 0;
    for (int i = 0; i < program.commands.size(); ++i) {
        result << state;
        const auto &c = program.commands[i];
        if (completeDialogue(program, i)) {
            const auto after = CounterState{state.known && state.arm < 0, state.arm, state.target};
            for (int n = 0; n < 6; ++n)
                result << CounterState{false, state.arm, state.target};
            state = after;
            queriesSinceYield = 1; // Completion query succeeds before resume/close.
            i += 6;
            continue;
        }
        if (c.opcode == 0x80000000)
            queriesSinceYield = 0;
        if (c.opcode == 1)
            state = {state.known, i, 0};
        else if (c.opcode == 2)
            state = {state.known, -1, 0};
        else if (c.opcode == 0xe || c.opcode == 0xb5) {
            if (++queriesSinceYield > 1 || state.arm < 0 ||
                (c.operands[0] != 0 && c.operands[0] != 2) || qint32(c.operands[1]) < state.target)
                state.known = false;
            state.target = qint32(c.operands[1]);
        } else if (uncertainControl(c))
            state.known = false;
        else if (state.arm >= 0 && c.opcode == 0x80000000 &&
                 (i + 1 == program.commands.size() || (program.commands[i + 1].opcode != 0xe &&
                                                       program.commands[i + 1].opcode != 0xb5)))
            state.known = false;
    }
    return result;
}
QVector<int> remainingGates(const DirectorProgram &program, int before, qint64 previous) {
    QVector<int> gates;
    for (int i = before; i < program.commands.size(); ++i) {
        const auto &c = program.commands[i];
        if (c.opcode == 2)
            return gates;
        if (c.opcode == 1 || c.opcode == 0x7d || c.opcode == 0x80000001 || uncertainControl(c))
            break;
        if (c.opcode == 0xe || c.opcode == 0xb5) {
            const qint64 target = qint32(c.operands[1]);
            if (i == 0 || program.commands[i - 1].opcode != 0x80000000 ||
                (c.operands[0] != 0 && c.operands[0] != 2) || target < previous)
                break;
            previous = target;
            gates << i;
        }
    }
    throw Error("The shared counter has no unambiguous reset after this position.");
}
void shiftGates(DirectorProgram &program, const QVector<int> &gates, qint64 delta) {
    for (int i : gates) {
        const auto target = qint64(qint32(program.commands[i].operands[1])) + delta;
        if (target < 0 || target > 0x7fffffff)
            throw Error("This wait would move a counter deadline outside its supported range.");
        program.commands[i].operands[1] = quint32(target);
    }
}
int setupEnd(const DirectorProgram &program) {
    int lead = 0;
    const auto &c = program.commands;
    while (lead < c.size() && (c[lead].opcode == 0x80000006 || c[lead].opcode == 0x80000007))
        ++lead;
    if ((program.resourceClass() == 4 || program.resourceClass() == 5) && lead + 1 < c.size() &&
        ((c[lead].opcode == 0x35 && c[lead + 1].opcode == 0x36) ||
         (c[lead].opcode == 0x36 && c[lead + 1].opcode == 0x35)))
        lead += 2;
    return lead;
}
} // namespace action_detail
namespace {
QString waitReason(const DirectorProgram &p, int before, const action_detail::CounterState &state) {
    if (!state.known)
        return "This position has branch-dependent counter state.";
    try {
        if (state.arm >= 0)
            action_detail::remainingGates(p, before, state.target);
        // The recovered failure scanner visits raw operand words too. Require its
        // first yield and immediately preceding barrier to be actual commands.
        const int first = p.commands[before].word;
        for (int word = first; word < p.source.size() / 4; ++word) {
            if (u32(p.source, word * 4) != 0x80000000)
                continue;
            for (int i = before + 1; i < p.commands.size(); ++i)
                if (p.commands[i].word == word && p.commands[i - 1].opcode == 0x80000002)
                    return {};
            return "The next yield has no adjacent condition barrier; adding a wait could restart "
                   "the scene.";
        }
    } catch (const std::exception &e) {
        return QString::fromUtf8(e.what());
    }
    return "There is no later yield to retain a pending wait.";
}
std::optional<quint32> spriteReference(const DirectorCommand &c) {
    const bool narrow = c.opcode == 0x46 || c.opcode == 0x63 || c.opcode == 0x64;
    if (!narrow && c.opcode != 0x65 && c.opcode != 0x66)
        return {};
    auto value = c.operands[c.opcode == 0x65 ? 2 : 0];
    if ((value & 0xffffff00U) == 0x08880000U)
        return value; // Keep the launch translation, not its token's low byte.
    if (narrow)
        value = quint8(value);
    return value < 30 ? std::optional<quint32>(value) : std::nullopt;
}
DirectorCommand command(quint32 op, QVector<quint32> operands = {}) {
    return {0, op, std::move(operands)};
}
} // namespace
QMap<quint32, QString> SceneActions::kinds() {
    return {{3, "Set Actor pose and position"},
            {0x14, "Create Actor"},
            {0x2a, "Create class Actor"},
            {7, "Move Actor"},
            {0x35, "Background camera"},
            {0x36, "Actor camera"},
            {0x1b, "Screen fade"},
            {0x1d, "Scale Actor"},
            {0x48, "Actor opacity"},
            {0xe, "Wait"},
            {0xb5, "Wait (A button can skip)"},
            {0xbf, "Dialogue"},
            {0x13, "Release Actor"},
            {0x15, "Turn Actor"},
            {0x1c, "Room Actor channel"},
            {0x46, "Create scene sprite"},
            {0x63, "Create rotated scene sprite"},
            {RotateSprite, "Rotate scene sprite"},
            {0x64, "Set scene sprite pose"},
            {0x66, "Release scene sprite"}};
}
QVector<SceneActionPosition> SceneActions::positions(const DirectorProgram &p) {
    QVector<SceneActionPosition> result;
    const auto actions = inspect(p);
    const auto states = action_detail::counterStates(p);
    const int lead = action_detail::setupEnd(p);
    for (int i = lead; i < p.commands.size(); ++i) {
        const auto &c = p.commands[i];
        QString name;
        for (const auto &a : actions)
            if (a.command == i) {
                name = a.name;
                break;
            }
        if (c.opcode == 0x7d || (c.opcode == 0x39 && c.operands == QVector<quint32>{0}))
            name = "scene exit";
        if (name.isEmpty())
            continue;
        const auto reason = waitReason(p, i, states[i]);
        result << SceneActionPosition{i, QString("Before %1 · word %2").arg(name).arg(c.word),
                                      reason.isEmpty(), reason};
    }
    return result;
}
DirectorProgram SceneActions::insert(const DirectorProgram &p, int before, quint32 kind,
                                     int ticks) {
    const auto points = positions(p);
    const auto point = std::find_if(points.begin(), points.end(),
                                    [before](const auto &v) { return v.command == before; });
    if (point == points.end() || !kinds().contains(kind))
        throw Error("Choose a supported action and insertion position.");
    if (kind == 0x2a && p.resourceClass() != 1 && p.resourceClass() != 8)
        throw Error("Create class Actor requires a fresh environment scene (class 1 or 8).");
    auto result = p;
    QVector<DirectorCommand> added;
    if (kind == 0xbf) {
        if (!point->canWait)
            throw Error(point->waitReason);
        const auto state = action_detail::counterStates(p)[before];
        if (state.arm >= 0)
            throw Error(
                "Insert dialogue outside the active counter span; input has no fixed duration.");
        QSet<quint32> used;
        for (const auto &c : p.commands) {
            if (c.opcode == 0xbf || c.opcode == 5 || c.opcode == 6)
                used << (c.operands[0] & 255);
            if (c.opcode == 0x10)
                used << (c.operands[2] & 255);
        }
        quint32 id = 0;
        while (id < 256 && used.contains(id))
            ++id;
        if (id == 256)
            throw Error("Every dialogue window ID is referenced; edit an existing window.");
        added << command(0xbf, {id, 5, 0, 0, 7, 160, 0, 1, 0, 1, 0, 0, 1}) << command(0)
              << command(0x80000002) << command(0x80000000) << command(0x10, {0, 2, id})
              << command(6, {id}) << command(5, {id});
    } else if (kind == 0xe || kind == 0xb5) {
        if (!point->canWait)
            throw Error(point->waitReason);
        if (ticks < 1)
            throw Error("A wait requires at least one update.");
        const auto state = action_detail::counterStates(p)[before];
        const qint64 target = state.target + ticks;
        if (target > 0x7fffffff)
            throw Error("The wait exceeds the counter range.");
        if (state.arm >= 0)
            action_detail::shiftGates(
                result, action_detail::remainingGates(p, before, state.target), ticks);
        else
            added << command(1);
        added << command(0) << command(0x80000002) << command(0x80000000)
              << command(kind, {2, quint32(target)});
        if (state.arm < 0)
            added << command(2);
    } else {
        quint32 slot = 0, facing = 0;
        for (int i = before - 1; i >= 0; --i) {
            const auto &c = p.commands[i];
            if ((c.opcode == 3 || c.opcode == 0x14 || c.opcode == 7 || c.opcode == 0x15 ||
                 c.opcode == 0x1c || c.opcode == 0x1d || c.opcode == 0x48 || c.opcode == 0x2a) &&
                c.operands[0] < 28) {
                slot = c.operands[0];
                break;
            }
        }
        for (int i = before - 1; i >= 0; --i) {
            const auto &c = p.commands[i];
            if ((c.opcode == 3 || c.opcode == 0x14 || c.opcode == 0x15) && c.operands[0] == slot) {
                const auto value = c.operands[c.opcode == 0x15 ? 2 : 3];
                if (value < 4) {
                    facing = value;
                    break;
                }
            }
        }
        quint32 spriteSlot = 0;
        for (int i = before - 1; i >= 0; --i) {
            if (const auto reference = spriteReference(p.commands[i])) {
                spriteSlot = *reference;
                break;
            }
        }
        QVector<quint32> operands;
        switch (kind) {
        case 0x46:
        case 0x63: {
            QSet<quint32> used;
            for (const auto &c : p.commands)
                if (const auto reference = spriteReference(c)) {
                    if (*reference >= 30)
                        throw Error("A sprite slot is selected at launch; an unused slot cannot be "
                                    "chosen automatically.");
                    used << *reference;
                }
            spriteSlot = 0;
            while (spriteSlot < 30 && used.contains(spriteSlot))
                ++spriteSlot;
            if (spriteSlot == 30)
                throw Error("All 30 sprite slots are already referenced; select an existing sprite "
                            "action to edit.");
            operands = kind == 0x46
                           ? QVector<quint32>{spriteSlot, 30, 1, 2, 0, 0, 0, 100, 0}
                           : QVector<quint32>{spriteSlot,  30,          1,          0, 0, 0, 180,
                                              quint32(-1), quint32(-1), quint32(-1)};
            break;
        }
        case RotateSprite:
            operands = {spriteSlot, quint32(-1), quint32(-1), 0,           0,
                        0,          180,         quint32(-1), quint32(-1), 0};
            break;
        case 0x64:
            operands = {spriteSlot, quint32(-1), quint32(-1), 0};
            break;
        case 0x66:
            operands = {spriteSlot};
            break;
        case 0x13:
            operands = {slot};
            break;
        case 0x15:
            operands = {slot, facing, (facing + 1) % 4, 1, 30};
            break;
        case 0x1c:
            operands = {slot, 0};
            break;
        case 3:
            operands = {slot,           quint32(-1),    quint32(-1),    quint32(-1),
                        quint32(-1000), quint32(-1000), quint32(-1000), quint32(-1)};
            break;
        case 0x14:
        case 0x2a: {
            QSet<quint32> used;
            for (const auto &c : p.commands) {
                const auto names = Directors::grammar()[c.opcode].operands;
                for (int i = 0; i < names.size(); ++i)
                    if (names[i].contains("actor_slot") || names[i] == "actor_selector")
                        used << c.operands[i];
                if (c.opcode == 7 || c.opcode == 0x1c || c.opcode == 0xa6)
                    used << c.operands[0];
            }
            slot = 0;
            while (slot < 28 && used.contains(slot))
                ++slot;
            if (slot == 28)
                throw Error(
                    "All 28 Actor slots are already referenced; select an existing Actor to edit.");
            operands = {slot, 1, 0, 0, 0, 0, 0, 0, 0};
            break;
        }
        case 7:
            operands = {slot, quint32(-1000), quint32(-1000), 0, 0, 30, 0};
            break;
        case 0x1b:
            operands = {30, 0, 0, 0, quint32(-1), 255};
            break;
        case 0x1d:
            operands = {slot, 100};
            break;
        case 0x48:
            operands = {slot, 255};
            break;
        case 0x35:
        case 0x36:
            operands = {0, 0, 0, 0, 0, 100000, 45000};
            for (int i = 0; i < before; ++i)
                if (p.commands[i].opcode == kind)
                    operands = p.commands[i].operands;
            break;
        }
        if (kind == 0x2a)
            added << command(0x14, operands) << command(0x2a, {slot, 2, 0, 0, 0, 2});
        else
            added << command(kind == RotateSprite ? 0x63 : kind, operands);
    }
    for (int i = 0; i < added.size(); ++i)
        result.commands.insert(before + i, added[i]);
    return DirectorProgram::decode(result.encode());
}
DirectorProgram SceneActions::move(const DirectorProgram &p, int index, int before) {
    const auto actions = inspect(p);
    const auto source = std::find_if(actions.begin(), actions.end(),
                                     [index](const auto &a) { return a.command == index; });
    const auto points = positions(p);
    const auto destination = std::find_if(points.begin(), points.end(),
                                          [before](const auto &v) { return v.command == before; });
    if (source == actions.end() || destination == points.end())
        throw Error("Select an action and a supported destination.");
    if (before >= index && before <= index + source->commandCount)
        return p;
    // Keep event/branch blocks intact. Relocation only crosses complete, known actions.
    const int end = std::max(index + source->commandCount, before);
    for (int cursor = std::min(index, before); cursor < end;) {
        const auto next = std::find_if(actions.begin(), actions.end(),
                                       [cursor](const auto &a) { return a.command == cursor; });
        if (next == actions.end())
            throw Error("This move crosses a separate native control section; edit that section in "
                        "Commands.");
        cursor += next->commandCount;
    }
    auto result = remove(p, index);
    if (before > index)
        before -= source->commandCount;
    if (source->commandCount == 4 || source->commandCount == 6) {
        const auto &field = source->fields.first();
        const auto &query = p.commands[index + field.commandOffset];
        const int duration = int(qint64(qint32(query.operands[1])) - field.bias);
        result = insert(result, before, query.opcode, duration);
        const int gate = result.commands[before].opcode == 1 ? before + 4 : before + 3;
        result.commands[gate].operands[0] = query.operands[0];
    } else {
        const auto remaining = positions(result);
        const auto point = std::find_if(remaining.begin(), remaining.end(),
                                        [before](const auto &v) { return v.command == before; });
        if (point == remaining.end())
            throw Error("Removing this action changes the destination's control boundary.");
        if (source->commandCount == 7 &&
            (!point->canWait || action_detail::counterStates(result)[before].arm >= 0))
            throw Error("Dialogue must remain outside an active counter span at a supported wait "
                        "boundary.");
        for (int i = 0; i < source->commandCount; ++i)
            result.commands.insert(before + i, p.commands[index + i]);
    }
    return DirectorProgram::decode(result.encode());
}
DirectorProgram SceneActions::remove(const DirectorProgram &p, int index) {
    if (index < action_detail::setupEnd(p))
        throw Error("The scene's initial setup must remain before its actions.");
    for (const auto &a : inspect(p))
        if (a.command == index) {
            auto result = p;
            if (p.commands[index].opcode == 0xbf && a.commandCount != 7)
                throw Error(
                    "This window has a separate control layout; use Commands to remove it.");
            if (a.commandCount == 6 || a.commandCount == 7) {
                const auto state = action_detail::counterStates(p)[index];
                if (!state.known || state.arm >= 0)
                    throw Error("This action follows uncertain counter state; use Commands to "
                                "change its structure.");
            }
            if (a.commandCount == 4) {
                const auto state = action_detail::counterStates(p)[index];
                const qint64 duration = qint32(p.commands[index + 3].operands[1]) - state.target;
                action_detail::shiftGates(
                    result, action_detail::remainingGates(p, index + 4, state.target + duration),
                    -duration);
            }
            // Operands can be scanner targets. Removing one must not silently change
            // unrelated native control flow; leave that unusual case in Commands.
            for (int i = index; i < index + a.commandCount; ++i)
                for (auto value : p.commands[i].operands)
                    if (value == 0x80000000 || value == 0x80000002 || value == 0x80000005)
                        throw Error("This action contains native scan markers; use Commands to "
                                    "edit its structure.");
            result.commands.remove(index, a.commandCount);
            return DirectorProgram::decode(result.encode());
        }
    throw Error("Select an action to remove.");
}
} // namespace lordly
