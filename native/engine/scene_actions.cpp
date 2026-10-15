#include "engine/scene_actions.h"
#include "engine/scene_action_control.h"
#include <QRegularExpression>
#include <algorithm>
#include <limits>
namespace lordly {
bool SceneActionField::canonical(quint32 word) const {
    if ((word & 0xffffff00U) == 0x08880000U)
        return false; // A launch translation is not an editable literal.
    const auto value = qint64(qint32(word)) - bias;
    return value >= minimum && value <= maximum && (choices.isEmpty() || choices.contains(value));
}
QString SceneActionField::text(quint32 word) const {
    return scale == 1 ? QString::number(qint64(qint32(word)) - bias)
                      : QString::number(double(qint32(word)) / scale, 'f', 3);
}
quint32 SceneActionField::parse(const QString &text) const {
    const auto input = text.trimmed();
    qint64 value = 0;
    bool valid = false;
    if (scale == 1)
        value = input.toLongLong(&valid, input.startsWith("0x", Qt::CaseInsensitive) ? 16 : 10);
    else {
        const auto match = QRegularExpression("^([+-]?)([0-9]+)(?:\\.([0-9]{1,3}))?$").match(input);
        if (match.hasMatch()) {
            const auto whole = match.captured(2).toLongLong(&valid);
            const auto fraction = match.captured(3).leftJustified(3, '0').toInt();
            if (valid && whole <= (std::numeric_limits<qint64>::max() - fraction) / scale) {
                value = whole * scale + fraction;
                if (match.captured(1) == "-")
                    value = -value;
            } else
                valid = false;
        }
    }
    if (!valid || value < minimum || value > maximum)
        throw Error(
            scale == 1
                ? "Enter a whole number within this field's range."
                : "Enter a position or angle with at most three decimal places within its range.");
    if (!choices.isEmpty() && !choices.contains(value)) {
        QStringList labels;
        for (auto choice : choices)
            labels << QString::number(choice);
        throw Error("Choose one of: " + labels.join(", "));
    }
    return quint32(value + bias);
}
QVector<SceneAction> SceneActions::inspect(const DirectorProgram &program) {
    QVector<SceneAction> result;
    const auto &commands = program.commands;
    const auto choices = SceneActions::choices(program);
    const auto counterStates = action_detail::counterStates(program);
    for (int i = 0; i < commands.size(); ++i) {
        const auto &c = commands[i];
        SceneAction action{i, 1, {}, {}};
        const auto choice = std::find_if(choices.begin(), choices.end(),
                                         [i](const auto &v) { return v.command == i; });
        auto field = [&](QString name, int operand, int scale = 1, qint64 minimum = -2147483648LL,
                         qint64 maximum = 2147483647LL, int offset = 0) {
            action.fields << SceneActionField{name, offset, operand, scale, minimum, maximum};
        };
        if (action_detail::completeImageTransitionWait(program, i)) {
            action.name = "Wait for image transition";
            action.commandCount = 4;
            action.timing = SceneActionTiming::Completion;
            result << action;
            i += 3;
            continue;
        }
        bool wait = i + 5 < commands.size() && c.opcode == 1 && commands[i + 1].opcode == 0 &&
                    commands[i + 2].opcode == 0x80000002 && commands[i + 3].opcode == 0x80000000 &&
                    (commands[i + 4].opcode == 0xe || commands[i + 4].opcode == 0xb5) &&
                    commands[i + 4].operands.size() == 2 && commands[i + 5].opcode == 2;
        if (wait) {
            const auto &query = commands[i + 4];
            wait = (query.operands[0] == 0 || query.operands[0] == 2) && query.operands[1] > 0 &&
                   query.operands[1] <= 0x7fffffff;
        }
        if (wait) {
            action.name = commands[i + 4].opcode == 0xb5 ? "Wait (A button can skip)" : "Wait";
            action.commandCount = 6;
            action.timing = SceneActionTiming::Counter;
            field("Duration (updates)", 1, 1, 1, 2147483647, 4);
            result << action;
            i += 5;
            continue;
        }
        const auto &state = counterStates[i];
        bool sharedWait = state.known && state.arm >= 0 && i + 3 < commands.size() &&
                          c.opcode == 0 && commands[i + 1].opcode == 0x80000002 &&
                          commands[i + 2].opcode == 0x80000000 &&
                          (commands[i + 3].opcode == 0xe || commands[i + 3].opcode == 0xb5) &&
                          (commands[i + 3].operands[0] == 0 || commands[i + 3].operands[0] == 2) &&
                          qint32(commands[i + 3].operands[1]) > state.target;
        if (sharedWait) {
            try {
                action_detail::remainingGates(program, i + 4, qint32(commands[i + 3].operands[1]));
            } catch (const Error &) {
                sharedWait = false;
            }
        }
        if (sharedWait) {
            action.name = commands[i + 3].opcode == 0xb5 ? "Wait (shared counter, A can skip)"
                                                         : "Wait (shared counter)";
            action.commandCount = 4;
            action.timing = SceneActionTiming::SharedCounter;
            field("Duration (updates)", 1, 1, 1, 2147483647 - state.target, 3);
            action.fields[0].bias = state.target;
            result << action;
            i += 3;
            continue;
        }
        switch (c.opcode) {
        case 3:
        case 7:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x1c:
        case 0x1d:
        case 0x48:
        case 0x2a:
            action.actorOperand = 0;
            break;
        case 0xbf:
            action.actorOperand = 3;
            break;
        default:
            break;
        }
        if (c.opcode == 0x14 && i + 1 < commands.size() &&
            (program.resourceClass() == 1 || program.resourceClass() == 8) &&
            commands[i + 1].opcode == 0x2a && c.operands[0] < 28 &&
            commands[i + 1].operands[0] == c.operands[0]) {
            action.name = "Create class Actor";
            action.commandCount = 2;
            field("Actor slot", 0, 1, 0, 27);
            action.fields.last().mirrors = {{1, 0}};
            field("Primary class", 1, 1, 1, 255, 1);
            field("Current class", 5, 1, 1, 255, 1);
            field("Pose ID", 2, 1, 0, 32767, 1);
            field("X", 4, 1000);
            field("Y", 5, 1000);
            field("Z", 6, 1000);
            field("Facing", 3, 1, 0, 4);
            field("Depth flag", 7, 1, 0, 255);
            // The temporary ordinary pose and independent body flags are retained.
            result << action;
            ++i;
            continue;
        }
        switch (c.opcode) {
        case 0x3a:
            action.name = "Construct scene image";
            field("Drawing channel", 0, 1, 1, 19);
            field("X", 1);
            field("Y", 2);
            field("Horizontal scale (%)", 4);
            field("Vertical scale (%)", 5);
            field("Vignette opacity cap", 6, 1, 0, 255);
            field("Orientation preset (0 or 8)", 7, 1, 0, 8);
            action.fields.last().choices = {0, 8};
            // Preserve the ignored payload and unrecognized orientation flags.
            break;
        case 0x50:
            action.name = "Image transition";
            field("Direction (0 forward, 1 reverse)", 0, 1, 0, 1);
            field("Duration (updates)", 1, 1, 1, 255);
            field("Echo tail (5-update units)", 2, 1, 0, 255);
            break;
        case 0x76:
            action.name = "Pan and zoom image";
            field("Start X", 0);
            field("Start Y", 1);
            field("Target X", 3);
            field("Target Y", 4);
            field("Zoom (0 normal, 1 reduced)", 5, 1, 0, 1);
            field("Duration (updates)", 6, 1, 1, 2147483647);
            // Operand2 is unused by the native starter and remains unchanged.
            break;
        case 0x49:
            action.name = "Create path ribbon";
            field("Path slot", 0, 1, 0, 19);
            field("Resource group", 1, 1, 0);
            field("Path entry", 2, 1, 0);
            field("Reveal through (%) (-1 uses full path)", 3, 1, -1, 100);
            field("Endpoint point (-1 uses percentage)", 4, 1, -1, 15);
            field("Width", 5, 1, 1, 5);
            field("Palette", 6, 1, 0, 3);
            field("Show immediately (0 or 1)", 7, 1, 0, 1);
            field("Drawing channel", 8, 1, 0, 255);
            break;
        case 0x4a:
            action.name = "Set path ribbon endpoint";
            field("Path slot", 0, 1, 0, 19);
            field("Percentage (-1 keeps current)", 1, 1, -1, 100);
            field("Endpoint point (-1 writes percentage)", 2, 1, -1, 15);
            break;
        case 0x4b:
            action.name = "Release path ribbon";
            field("Path slot (-1 selects all)", 0, 1, -1, 19);
            field("Release mode (0 immediate, 1 fade)", 1, 1, 0, 1);
            break;
        case 0x46:
            action.name = "Create scene sprite";
            field("Sprite slot", 0, 1, 0, 29);
            field("Art bank", 1, 1, 1, 32767);
            field("Animation", 2, 1, 0, 32767);
            field("Facing", 3, 1, 0, 9);
            field("X", 4);
            field("Y", 5);
            field("Drawing channel", 6, 1, 0, 255);
            field("Scale (%)", 7, 1, -32768, 32767);
            field("Variant", 8, 1, 0, 255);
            break;
        case 0x63: {
            // A launch-selected route or creation mode has no fixed field layout.
            if ((c.operands[8] & 0xffffff00U) == 0x08880000U ||
                (c.operands[9] & 0xffffff00U) == 0x08880000U)
                continue;
            const bool path = c.operands[8] != quint32(-1);
            action.name =
                c.operands[9] == quint32(-1)
                    ? (path ? "Create sprite with path heading" : "Create rotated scene sprite")
                    : (path ? "Set sprite heading from path" : "Rotate scene sprite");
            field("Sprite slot", 0, 1, 0, 29);
            if (c.operands[9] == quint32(-1)) {
                field("Art bank", 1, 1, 1, 32767);
                field("Animation", 2, 1, 0, 32767);
                field("Variant", 3, 1, 0, 255);
                field("X", 4);
                field("Y", 5);
            }
            if (path) {
                field("Resource group (-1 uses an active path)", 7, 1, -1);
                field("Path entry / active path slot", 8, 1, 0);
            } else
                field("Rotation (180 = unrotated)", 6, 1, -32768, 32767);
            break;
        }
        case 0x64:
            action.name = "Set scene sprite pose";
            field("Sprite slot", 0, 1, 0, 29);
            field("Art bank (-1 keeps current)", 1, 1, -1, 32767);
            field("Animation (-1 keeps current)", 2, 1, -1, 32767);
            field("Variant", 3, 1, 0, 255);
            break;
        case 0x66:
            action.name = "Release scene sprite";
            field("Sprite slot (-1 selects all)", 0, 1, -1, 29);
            break;
        case 0x2a:
            action.name = "Set Actor class and pose";
            field("Actor slot (-1 skips)", 0, 1, -1, 27);
            field("Primary class (-1 keeps current)", 1, 1, -1, 255);
            field("Current class (-1 keeps existing)", 5, 1, -1, 255);
            field("Pose ID (-1 keeps current)", 2, 1, -1, 32767);
            // Keep the independent body flags exactly as authored. Their two
            // operands are not interchangeable with ordinary Actor facing.
            break;
        case 0x13:
            action.name = "Release Actor";
            field("Actor slot (-1 selects all)", 0, 1, -1, 27);
            break;
        case 0x15:
            action.name = "Turn Actor";
            field("Actor slot", 0, 1, 0, 27);
            field("Start facing", 1, 1, 0, 3);
            field("Target facing", 2, 1, 0, 3);
            field("Turn phase direction", 3, 1, -1, 1);
            action.fields.last().choices = {-1, 1};
            field("Duration budget (updates)", 4, 1, 1, 32767);
            break;
        case 0x1c:
            action.name = "Room Actor channel";
            field("Actor slot (-1 selects all)", 0, 1, -1, 27);
            field("Drawing channel", 1, 1, 0, 19);
            break;
        case 0xbf:
            action.name = "Dialogue window";
            if (choice != choices.end()) {
                action.name = "Choice";
                action.commandCount = choice->end - i;
                action.timing = SceneActionTiming::Completion;
                field("Window ID", 0, 1, 0, 255);
                action.fields.last().mirrors = {
                    {6, 0}, {10, 0}, {15, 2}, {choice->secondBegin - 1 - i, 2}};
            } else if (action_detail::completeDialogue(program, i)) {
                action.name = "Dialogue";
                action.commandCount = 7;
                action.timing = SceneActionTiming::Completion;
                field("Window ID", 0, 1, 0, 255);
                action.fields.last().mirrors = {{4, 2}, {5, 0}, {6, 0}};
            }
            field("Text archive", 1, 1, 0, 502);
            field("Text entry (0x8000 selects alternate table)", 2, 1, 0, 65535);
            field("Owner Actor slot", 3, 1, 0, 27);
            field("X / horizontal preset", 4, 1, 0, 255);
            field("Y / vertical preset", 5, 1, -32768, 32767);
            field("Portrait side", 7, 1, 0, 1);
            field("Mirror portrait", 8, 1, 0, 1);
            field("Pointer edge", 9, 1, 0, 1);
            field("Portrait class (-1 uses Actor, 0 hides)", 10, 1, -1, 255);
            field("Portrait variant", 11, 1, 0, 1);
            field("Placement (0 follows Actor, 1 fixed)", 12, 1, 0, 1);
            break;
        case 3:
        case 0x14:
            action.name = c.opcode == 3 ? "Set Actor pose and position" : "Create Actor";
            field("Actor slot", 0, 1, c.opcode == 3 ? -1 : 0, 27);
            field("Art bank", 1);
            field("Animation", 2);
            field("Facing", 3);
            field("X", 4, 1000);
            field("Y", 5, 1000);
            field("Z", 6, 1000);
            if (c.opcode == 0x14)
                field("Depth flag", 7, 1, 0, 255);
            field("Variant", c.opcode == 3 ? 7 : 8);
            break;
        case 7:
            if (!c.operands[5] && !c.operands[6] &&
                (c.operands[1] == c.operands[3] || (c.operands[3] == quint32(-1000) && c.operands[4] == quint32(-1000) && c.operands[1] == quint32(-999))) &&
                c.operands[2] == c.operands[4]) {
                action.name = "Place Actor";
                field("Actor slot", 0, 1, 0, 27);
                field("X", 3, 1000); action.fields.last().mirrors = {{0, 1}};
                field("Z", 4, 1000); action.fields.last().mirrors = {{0, 2}};
                break;
            }
            action.name = "Move Actor";
            field("Actor slot", 0, 1, 0, 27);
            field("Start X", 1, 1000);
            field("Start Z", 2, 1000);
            field("Target X", 3, 1000);
            field("Target Z", 4, 1000);
            field("Duration (updates)", 5);
            field("Speed", 6);
            break;
        case 0x1b:
            action.name = "Screen fade";
            field("Duration (updates)", 0, 1, 0, 65535);
            field("Red", 1, 1, 0, 255);
            field("Green", 2, 1, 0, 255);
            field("Blue", 3, 1, 0, 255);
            field("Start opacity (-1 keeps current)", 4, 1, -1, 255);
            field("Target opacity", 5, 1, 0, 255);
            break;
        case 0x1d:
        case 0x48:
            action.name = c.opcode == 0x1d ? "Scale Actor" : "Actor opacity";
            field("Actor slot (-1 selects all)", 0, 1, -1, 27);
            field(c.opcode == 0x1d ? "Scale (%)" : "Opacity", 1, 1,
                  c.opcode == 0x1d ? -2147483648LL : 0, c.opcode == 0x1d ? 2147483647LL : 255);
            break;
        case 0x2c:
            action.name = "Camera pan and zoom";
            field("Translation X", 0, 1000);
            field("Translation Y", 1, 1000);
            field("Scale X", 2, 1000);
            field("Scale Y", 3, 1000);
            field("Duration (updates)", 4, 1, 1, 65535);
            break;
        case 0x35:
        case 0x36:
            action.name = c.opcode == 0x36 ? "Actor camera" : "Background camera";
            field("Target X", 0, 1000);
            field("Target Y", 1, 1000);
            field("Target Z", 2, 1000);
            field("Eye X", 3, 1000);
            field("Eye Y", 4, 1000);
            field("Eye Z", 5, 1000);
            field("Field of view (degrees)", 6, 1000);
            break;
        default:
            continue;
        }
        if (const int completion = action_detail::actionCompletion(program, i)) {
            action.commandCount = completion;
            action.timing = SceneActionTiming::Completion;
            action.waitsForCompletion = true;
            if (c.opcode == 7)
                action.fields[0].mirrors = {{4, 2}};
            if (c.opcode == 0x2c)
                action.fields[4].mirrors = {{5, 1}};
        }
        result << action;
        i += choice != choices.end() ? 13 : action.commandCount - 1;
    }
    return result;
}
std::optional<QPair<int, int>> SceneActions::ribbonSource(const DirectorProgram &program,
                                                          int before, quint32 slot) {
    const auto constructor = ribbonConstructor(program, before, slot);
    if (!constructor)
        return {};
    const auto &args = program.commands[*constructor].operands;
    for (int operand : {1, 2})
        if ((args[operand] & 0xffffff00U) == 0x08880000U || qint32(args[operand]) < 0)
            return {};
    return QPair<int, int>{int(args[1]), int(args[2])};
}
std::optional<int> SceneActions::ribbonConstructor(const DirectorProgram &program, int before,
                                                   quint32 slot) {
    if (slot >= 20)
        return {};
    const auto actions = inspect(program);
    for (int cursor = before; cursor > 0;) {
        const auto previous = std::find_if(actions.begin(), actions.end(), [cursor](const auto &a) {
            return a.command + a.commandCount == cursor;
        });
        if (previous == actions.end())
            return {};
        const auto &c = program.commands[previous->command];
        if (c.opcode == 0x49) {
            if ((c.operands[0] & 0xffffff00U) == 0x08880000U)
                return {}; // This constructor could replace the requested slot.
            if (c.operands[0] == slot)
                return previous->command;
        } else if (c.opcode == 0x4b && c.operands[1] != 1 &&
                   (c.operands[0] == slot || c.operands[0] == quint32(-1) ||
                    (c.operands[0] & 0xffffff00U) == 0x08880000U))
            return {};
        cursor = previous->command;
    }
    return {};
}
DirectorProgram SceneActions::edit(const DirectorProgram &program, int actionCommand, int field,
                                   const QString &value) {
    for (const auto &action : inspect(program))
        if (action.command == actionCommand) {
            if (field < 0 || field >= action.fields.size())
                throw Error("Select an action field");
            const auto &f = action.fields[field];
            const auto &source = program.commands[action.command + f.commandOffset];
            if (!f.canonical(source.operands[f.operand]))
                throw Error("This source encoding requires the Commands inspector");
            auto result = program;
            const auto replacement = f.parse(value);
            if (action.timing == SceneActionTiming::SharedCounter) {
                const qint64 target = qint32(source.operands[f.operand]);
                action_detail::shiftGates(
                    result, action_detail::remainingGates(program, action.command + 4, target),
                    qint64(qint32(replacement)) - target);
            }
            result.commands[action.command + f.commandOffset].operands[f.operand] = replacement;
            for (const auto &[offset, operand] : f.mirrors)
                result.commands[action.command + offset].operands[operand] = replacement;
            if (action.name == "Place Actor") {
                auto &args = result.commands[action.command].operands;
                args[1] = args[3] == quint32(-1000) && args[4] == quint32(-1000) ? quint32(-999) : args[3];
                args[2] = args[4];
            }
            return DirectorProgram::decode(result.encode());
        }
    throw Error("Select a supported scene action");
}
} // namespace lordly
