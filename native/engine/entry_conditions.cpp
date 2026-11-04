#include "engine/entry_conditions.h"
#include <algorithm>
#include <array>

namespace lordly {
namespace {
struct Value {
    int number = 0, flag = -1;
    bool inverted = false, literal = false;
    QMap<int, int> parts;
    bool chaos = false;
};
bool compare(int operation, int left, int right) {
    if (operation >= 12) { left = qint16(left); right = qint16(right); }
    switch (operation) {
    case 10: return left == right;
    case 11: return left != right;
    case 12: return left > right;
    case 13: return left < right;
    case 14: return left >= right;
    case 15: return left <= right;
    }
    throw Error("Unsupported entry comparison");
}
int reversedRelation(int relation) {
    return QMap<int, int>{{10, 10}, {11, 11}, {12, 13}, {13, 12}, {14, 15}, {15, 14}}.value(relation, -1);
}
int oppositeRelation(int relation) {
    return QMap<int, int>{{10, 11}, {11, 10}, {12, 15}, {13, 14}, {14, 13}, {15, 12}}.value(relation, -1);
}
void guard(const QByteArray &bytes, const QMap<int, int> &words) {
    for (auto it = words.begin(); it != words.end(); ++it)
        if (it.key() < 0 || it.key() + 2 > bytes.size() || u16(bytes, it.key()) != it.value())
            throw Error("This entry condition changed. Select the current branch again.");
}
void remember(EntryFlagCondition &condition, const QByteArray &bytes, int at) {
    condition.sourceWords[at] = u16(bytes, at);
}
}
EntryConditions EntryConditions::read(const Rom &rom, const SceneEvents &events,
                                      const ScenePrograms &scenes, const StoryRoute &route, int chaos) {
    rom.requireSupported();
    (void)scenes;
    if (rom.revision() || route.node < 1 || route.node > 53 || chaos < 0 || chaos > 100)
        throw Error("Unsupported world entry selection");
    const auto bytes = events.program(0);
    if (route.node * 2 + 2 > bytes.size()) throw Error("Missing world entry table");
    int pc = u16(SceneEvents::retailRow(rom, 0), route.node * 2);
    if (!pc) throw Error("This map location has no native entry");
    EntryConditions result;
    std::array<Value, 8> registers{};
    auto known = route.requirements;
    QMap<int, int> writes, pathWords;
    QMap<int, EntryFlagCondition> conditions;
    auto conditionFor = [&](int flag) -> EntryFlagCondition & {
        auto &condition = conditions[flag];
        condition.flag = flag; condition.value = route.requirements.value(flag);
        return condition;
    };
    for (int steps = 0; steps < 1000; ++steps) {
        if (pc < 0 || pc % 2 || pc + 2 > bytes.size()) throw Error("Invalid native world-entry path");
        const int operation = u8(bytes, pc), argument = u8(bytes, pc + 1);
        pathWords[pc] = u16(bytes, pc);
        bool jump = false;
        if (operation >= 0x68 && operation <= 0x6f) {
            registers[operation & 7] = {argument, -1, false, true, {{pc, 0}}};
        } else if (operation >= 0x88 && operation <= 0x8f) {
            auto &value = registers[operation & 7];
            value.number = (value.number & 255) | (argument << 8);
            if (value.literal) value.parts[pc] = 8;
            else { value.flag = -1; value.chaos = false; }
        } else if (operation >= 0x60 && operation <= 0x67) {
            Value value;
            if (argument == 0x61) {
                const auto token = registers[1];
                const int signedFlag = qint16(token.number), flag = qAbs(signedFlag);
                if (!known.contains(flag)) throw Error("This branch no longer describes the current native flag path");
                value.number = known.value(flag) ^ (signedFlag < 0);
                if (!writes.contains(flag) && route.requirements.contains(flag)) {
                    auto &condition = conditionFor(flag);
                    value.flag = flag; value.inverted = signedFlag < 0;
                    if (!token.literal || token.parts.isEmpty()) {
                        condition.unavailable = "The flag number is calculated rather than stored in a literal.";
                    } else for (auto part = token.parts.begin(); part != token.parts.end(); ++part) {
                        if (!condition.flagOffsets.contains(part.key())) condition.flagOffsets << part.key();
                        condition.literalShifts[part.key()] = part.value();
                        condition.literalInversions[part.key()] = signedFlag < 0;
                        remember(condition, bytes, part.key());
                    }
                }
            } else if (argument == 0xfa) value.number = 1;
            else if (argument == 0xf9) value.number = route.node;
            else if (argument == 0xff) { value.number = chaos; value.chaos = true; }
            else throw Error("Unsupported property in this native entry");
            registers[operation & 7] = value;
        } else if (operation == 0x71 && argument == 0x60) {
            const int flag = registers[0].number, value = registers[1].number == 1;
            known[flag] = writes[flag] = value;
        } else if (operation == 0x91 && argument == 2) {
            registers[1] = {registers[1].number & registers[2].number};
        } else if (operation == 1) jump = true;
        else if (operation == 2) {
            pc += 2 + 2 * qint16((argument << 8) | (registers[0].number & 255));
            continue;
        } else if (operation >= 0xa0 && operation <= 0xff &&
                   QSet<int>{1, 2, 3, 6, 7, 11, 12, 13, 14, 15}.contains(operation & 15)) {
            const int low = operation & 15, relation = operation >> 4;
            const auto &left = registers[low >= 12 ? low & 3 : (low >> 2) & 3];
            const Value zero{0, -1, false, true};
            const auto &right = low >= 12 ? zero : registers[low & 3];
            jump = compare(relation, left.number, right.number);
            if ((left.chaos && right.literal) || (right.chaos && left.literal)) {
                const auto &threshold = left.chaos ? right : left;
                int producer = -1;
                for (auto part = threshold.parts.begin(); part != threshold.parts.end(); ++part)
                    if (part.value() == 0) producer = part.key();
                if (producer >= 0) {
                    EntryNumberCondition condition;
                    condition.value = threshold.number;
                    condition.valueOffset = producer; condition.comparisonOffset = pc;
                    condition.reversedOperands = right.chaos; condition.branchTaken = jump;
                    condition.relation = right.chaos ? reversedRelation(relation) : relation;
                    if (!jump) condition.relation = oppositeRelation(condition.relation);
                    result.numbers << condition;
                }
            }
            for (const auto *subject : {&left, &right}) if (subject->flag >= 0) {
                auto &condition = conditionFor(subject->flag);
                const auto &other = subject == &left ? right : left;
                const bool constant = other.flag < 0 && other.literal;
                if (!constant || (relation != 10 && relation != 11) || (other.number != 0 && other.number != 1)) {
                    condition.stateEditable = false;
                    condition.unavailable = "This flag participates in a calculated comparison.";
                    continue;
                }
                if (!condition.comparisonOffsets.contains(pc)) condition.comparisonOffsets << pc;
                remember(condition, bytes, pc);
            }
        } else if (operation == 0x12) {
            if (argument != route.event || (registers[0].number & 255) != route.clearFrom ||
                (registers[0].number >> 8) != route.clearTo || writes != route.writes)
                throw Error("The selected branch no longer matches this native path");
            if (conditions.size() != route.requirements.size())
                throw Error("The selected branch contains stale flag requirements");
            result.event = argument; result.eventOffset = pc; result.sourceWords = pathWords;
            for (auto &condition : result.numbers) condition.sourceWords = pathWords;
            for (auto &condition : conditions) {
                condition.sourceWords = pathWords;
                std::sort(condition.flagOffsets.begin(), condition.flagOffsets.end());
                std::sort(condition.comparisonOffsets.begin(), condition.comparisonOffsets.end());
                if (condition.comparisonOffsets.isEmpty()) {
                    condition.stateEditable = false;
                    if (condition.unavailable.isEmpty()) condition.unavailable = "This flag is read without a direct Boolean comparison.";
                }
                result.flags << condition;
            }
            return result;
        } else if (operation == 0x2f) throw Error("The selected path now stays on the world map");
        else throw Error("Unsupported operation in this native entry");
        pc += 2 + (jump ? 2 * qint8(argument) : 0);
    }
    throw Error("Looping native world-entry path");
}
void EntryConditions::setFlag(SceneEvents &events, const ScenePrograms &scenes,
                               const EntryFlagCondition &condition, int flag) {
    if (flag < condition.minimum || flag > condition.maximum || condition.flagOffsets.isEmpty() || !condition.unavailable.isEmpty())
        throw Error("This flag literal cannot be safely retargeted");
    const auto bytes = events.program(0);
    guard(bytes, condition.sourceWords);
    auto next = events;
    for (int at : condition.flagOffsets) {
        const int token = quint16(condition.literalInversions.value(at) ? -flag : flag);
        const int shift = condition.literalShifts.value(at);
        const bool wide = !shift && at + 2 < bytes.size() && u8(bytes, at + 2) == u8(bytes, at) + 0x20;
        next.editParameter(0, at, "value", wide ? token : ((token >> shift) & 255), scenes);
    }
    events = std::move(next);
}
void EntryConditions::setState(SceneEvents &events, const ScenePrograms &scenes,
                                const EntryFlagCondition &condition, int state) {
    if ((state != 0 && state != 1) || !condition.stateEditable || condition.comparisonOffsets.isEmpty())
        throw Error("This entry does not have an editable Boolean flag test");
    guard(events.program(0), condition.sourceWords);
    if (state == condition.value) return;
    auto next = events;
    for (int at : condition.comparisonOffsets) {
        const int relation = u8(next.program(0), at) >> 4;
        if (relation != 10 && relation != 11) throw Error("The native comparison is no longer Boolean");
        next.editParameter(0, at, "comparison", relation == 10 ? 11 : 10, scenes);
    }
    events = std::move(next);
}
void EntryConditions::setNumberValue(SceneEvents &events, const ScenePrograms &scenes,
                                      const EntryNumberCondition &condition, int value) {
    if (value < condition.minimum || value > condition.maximum)
        throw Error("Choose a Chaos Frame threshold from 0 to 100");
    guard(events.program(0), condition.sourceWords);
    auto next = events;
    next.editParameter(0, condition.valueOffset, "value", value, scenes);
    events = std::move(next);
}
void EntryConditions::setNumberRelation(SceneEvents &events, const ScenePrograms &scenes,
                                         const EntryNumberCondition &condition, int relation) {
    if (relation < 10 || relation > 15) throw Error("Choose a supported Chaos Frame comparison");
    guard(events.program(0), condition.sourceWords);
    if (!condition.branchTaken) relation = oppositeRelation(relation);
    if (condition.reversedOperands) relation = reversedRelation(relation);
    auto next = events;
    next.editParameter(0, condition.comparisonOffset, "comparison", relation, scenes);
    events = std::move(next);
}
void EntryConditions::setEvent(SceneEvents &events, const ScenePrograms &scenes,
                                const EntryConditions &entry, int event) {
    guard(events.program(0), entry.sourceWords);
    if (entry.eventOffset < 0 || u8(events.program(0), entry.eventOffset) != 0x12)
        throw Error("This entry no longer has its native event-row switch");
    auto next = events;
    next.editParameter(0, entry.eventOffset, "operand", event, scenes);
    events = std::move(next);
}
} // namespace lordly
