#include "engine/scene_actions.h"
#include "engine/scene_action_control.h"
#include <QRegularExpression>
#include <limits>
namespace lordly {
bool SceneActionField::canonical(quint32 word) const {
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
    const auto counterStates = action_detail::counterStates(program);
    for (int i = 0; i < commands.size(); ++i) {
        const auto &c = commands[i];
        SceneAction action{i, 1, {}, {}};
        auto field = [&](QString name, int operand, int scale = 1, qint64 minimum = -2147483648LL,
                         qint64 maximum = 2147483647LL, int offset = 0) {
            action.fields << SceneActionField{name, offset, operand, scale, minimum, maximum};
        };
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
        switch (c.opcode) {
        case 0x2a:
            action.name = "Set Actor class and pose";
            field("Actor slot (-1 skips)", 0, 1, -1, 27);
            field("Primary class (-1 keeps current)", 1, 1, -1, 255);
            field("Secondary class (-1 keeps current)", 5, 1, -1, 255);
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
            if (action_detail::completeDialogue(program, i)) {
                action.name = "Dialogue";
                action.commandCount = 7;
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
        result << action;
        i += action.commandCount - 1;
    }
    return result;
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
            if (action.commandCount == 4) {
                const qint64 target = qint32(source.operands[f.operand]);
                action_detail::shiftGates(
                    result, action_detail::remainingGates(program, action.command + 4, target),
                    qint64(qint32(replacement)) - target);
            }
            result.commands[action.command + f.commandOffset].operands[f.operand] = replacement;
            for (const auto &[offset, operand] : f.mirrors)
                result.commands[action.command + offset].operands[operand] = replacement;
            return DirectorProgram::decode(result.encode());
        }
    throw Error("Select a supported scene action");
}
} // namespace lordly
