#include "engine/scene_timed_authoring.h"
#include <algorithm>
#include <cmath>
namespace lordly {
TimedSceneEdit insertSceneActionAtTime(const DirectorProgram &source, const QMap<int, int> &times,
                                      int tick, quint32 kind, int duration) {
    if (tick < 0) throw Error("Choose a nonnegative playback update");
    const auto points = SceneActions::positions(source);
    for (const auto &action : SceneActions::inspect(source)) {
        if (action.timing != SceneActionTiming::Counter && action.timing != SceneActionTiming::SharedCounter) continue;
        const int start = times.value(source.commands[action.command].word, -1);
        if (start < 0 || tick <= start) continue;
        const auto &field = action.fields.first();
        const auto updates = field.text(source.commands[action.command + field.commandOffset].operands[field.operand]).toLongLong();
        const int elapsed = tick - start;
        if (elapsed >= updates) continue;
        const int after = action.command + action.commandCount;
        if (after < source.commands.size() && times.contains(source.commands[after].word) && times[source.commands[after].word] <= tick) continue;
        // Split the existing wait around the new action. Shared-counter edits
        // adjust later gates; inserting the remainder restores their deadlines.
        auto result = SceneActions::edit(source, action.command, 0, QString::number(elapsed));
        const int waitKind = source.commands[action.command + field.commandOffset].opcode;
        const int beforeCount = int(result.commands.size());
        result = SceneActions::insert(result, after, kind, duration);
        const int inserted = int(result.commands.size()) - beforeCount;
        result = SceneActions::insert(result, after + inserted, waitKind, int(updates - elapsed));
        return {result, after};
    }
    int before = -1;
    for (const auto &point : points) {
        const int word = source.commands[point.command].word;
        if (times.contains(word) && times[word] == tick) before = point.command;
    }
    if (before < 0 && tick == 0 && !points.isEmpty()) before = points.first().command;
    if (before < 0)
        throw Error("The playhead is inside a condition or completion wait. Choose an executed action boundary or a timed Wait.");
    return {SceneActions::insert(source, before, kind, duration), before};
}
TimedSceneEdit placeSceneActorAtTime(const DirectorProgram &source, const QMap<int, int> &times,
                                    int tick, int slot, SceneVector position) {
    if (slot < 0 || slot >= 28 || !std::isfinite(position.x) || !std::isfinite(position.z))
        throw Error("Choose an existing Actor and finite position");
    const auto coordinate = [](double value) {
        const double scaled = std::round(value * 1000.);
        if (scaled < -2147483648. || scaled > 2147483647.) throw Error("Actor position exceeds game coordinates");
        return quint32(qint32(scaled));
    };
    const auto x = coordinate(position.x), z = coordinate(position.z);
    TimedSceneEdit result{source, -1};
    for (int i = 0; i < source.commands.size(); ++i) {
        const auto &c = source.commands[i];
        if (times.value(c.word, -1) == tick && c.opcode == 7 && c.operands[0] == quint32(slot) &&
            (c.operands[1] == c.operands[3] || (c.operands[3] == quint32(-1000) && c.operands[4] == quint32(-1000) && c.operands[1] == quint32(-999))) &&
            c.operands[2] == c.operands[4] && !c.operands[5] && !c.operands[6])
            result.command = i;
    }
    if (result.command < 0) result = insertSceneActionAtTime(source, times, tick, 7);
    // Native movement's already-at-destination path sets X/Z without changing
    // pose, body identity or height. It works for ordinary and class Actors.
    // (-1,-1) is a native retain-position sentinel. An explicit start 0.001
    // away remains inside revision0's 0.1 destination tolerance and avoids it.
    const auto startX = x == quint32(-1000) && z == quint32(-1000) ? quint32(-999) : x;
    result.program.commands[result.command].operands = {quint32(slot), startX, z, x, z, 0, 0};
    result.program = DirectorProgram::decode(result.program.encode());
    return result;
}
DirectorProgram holdSceneBeforeExit(const DirectorProgram &source, int exit,
                                    const QSet<int> &actors, bool fade, int panUpdates) {
    if (exit < 0 || exit >= source.commands.size() || source.commands[exit].opcode != 0x7d)
        throw Error("Choose an executed scene exit");
    const auto points = SceneActions::positions(source);
    const auto point = std::find_if(points.begin(), points.end(), [exit](const auto &v) { return v.command == exit; });
    if (point == points.end() || !point->canWait)
        throw Error("This scene exit is not a supported wait boundary");
    auto result = source;
    if (panUpdates > 0) {
        result = SceneActions::insert(result, exit, 0xe, panUpdates);
        exit += int(result.commands.size() - source.commands.size());
    }
    QVector<DirectorCommand> added;
    const auto wait = [&](quint32 opcode, QVector<quint32> operands) {
        added << DirectorCommand{0, 0, {}} << DirectorCommand{0, 0x80000002, {}}
              << DirectorCommand{0, 0x80000000, {}} << DirectorCommand{0, opcode, std::move(operands)};
    };
    if (fade) wait(0x7f, {0, 0});
    auto orderedSlots = actors.values(); std::sort(orderedSlots.begin(), orderedSlots.end());
    for (int slot : orderedSlots) {
        if (slot < 0 || slot >= 28) throw Error("Invalid moving Actor slot");
        wait(0x11, {0, 0, quint32(slot)});
    }
    for (int i = 0; i < added.size(); ++i) result.commands.insert(exit + i, added[i]);
    return DirectorProgram::decode(result.encode());
}
} // namespace lordly
