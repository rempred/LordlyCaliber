#include "engine/pose_bank.h"
#include "game/art/animation_readers.h"
#include <QMap>
#include <array>
namespace lordly {
namespace {
constexpr std::array<uint8_t, 23> widths{1, 3, 3, 2, 2, 2, 1, 1, 1, 1, 1,
                                     1, 4, 3, 3, 2, 3, 3, 3, 3, 3, 4, 3};
bool frame(const PoseRecord &r) {
    return r.opcode == 1 || r.opcode == 21;
}
PoseRecord frameRecord(int token, int ticks) {
    if (token < 0 || token > 65535 || ticks < 0 || ticks > 255)
        throw Error("Frame token or duration exceeds its stored range");
    QByteArray operands;
    operands += char(token);
    if (token > 255)
        operands += char(token >> 8);
    operands += char(ticks);
    return {token > 255 ? 21 : 1, operands};
}
void requireFrame(const QVector<PoseRecord> &records, int index) {
    if (index < 0 || index >= records.size() || !frame(records[index]))
        throw Error("Choose a frame instruction");
}
void remapJumps(QVector<PoseRecord> &records, const QVector<int> &mapping, int oldCount) {
    for (auto &record : records)
        if (record.opcode == 4) {
            const int target = u8(record.operands, 0);
            // Preserve implicit end-of-program jumps as the program grows or shrinks.
            const int replacement = target < oldCount    ? mapping[target]
                                    : target == oldCount ? int(records.size())
                                                         : target;
            if (replacement < 0 || replacement > 255)
                throw Error("Sequence jump exceeds its one-byte record index");
            record.operands[0] = char(replacement);
        }
}
} // namespace
PoseProgram PoseBank::parseProgram(const QByteArray &source) {
    PoseProgram result{source, {}};
    if (source.isEmpty())
        return result;
    std::array<LordlyPoseCommand, 255> commands;
    unsigned count = 0;
    if (lordly_pose_decode(reinterpret_cast<const uint8_t *>(source.constData()),
            size_t(source.size()), widths.data(), widths.size(), commands.data(), &count))
        throw Error("Actor pose has invalid instructions or unaccounted bytes");
    result.records.reserve(int(count));
    for (unsigned i = 0; i < count; ++i) {
        const auto &command = commands[i];
        result.records << PoseRecord{command.opcode,
            QByteArray(reinterpret_cast<const char *>(command.operands), command.width - 1)};
    }
    return result;
}
PoseBank PoseBank::parse(const QByteArray &bytes) {
    const auto first = u32(bytes, 0);
    if (!first || first % 4 || first > quint32(bytes.size()) || first / 4 > 32768)
        throw Error("Invalid actor pose directory extent");
    QVector<quint32> offsets;
    for (quint32 at = 0; at < first; at += 4) {
        const auto offset = u32(bytes, at);
        if (offset < first || offset > quint32(bytes.size()) ||
            (!offsets.isEmpty() && offset < offsets.last()))
            throw Error("Invalid actor pose directory pointer");
        offsets << offset;
    }
    offsets << quint32(bytes.size());
    PoseBank result;
    for (int i = 0; i + 1 < offsets.size(); ++i)
        result.programs_ << parseProgram(bytes.mid(offsets[i], offsets[i + 1] - offsets[i]));
    return result;
}
QByteArray PoseBank::encodeProgram(const QVector<PoseRecord> &records) {
    if (records.size() > 255)
        throw Error("A pose program can contain at most 255 instructions");
    QByteArray result(1, char(records.size()));
    for (const auto &record : records) {
        if (record.opcode < 0 || record.opcode >= int(widths.size()) ||
            record.operands.size() != widths[record.opcode] - 1)
            throw Error("Pose instruction has invalid operands");
        result += char(record.opcode);
        result += record.operands;
    }
    return result;
}
const PoseProgram &PoseBank::effective(int state) const {
    if (state < 0 || state >= programs_.size())
        throw Error("Actor pose state is outside its directory");
    while (state < programs_.size() && programs_[state].source.isEmpty())
        ++state;
    if (state == programs_.size())
        throw Error("Actor pose pointer is past its resource");
    return programs_[state];
}
QByteArray PoseBank::encoded() const {
    QByteArray result(programs_.size() * 4, 0);
    for (int i = 0; i < programs_.size(); ++i) {
        put32(result, i * 4, quint32(result.size()));
        result += programs_[i].source;
    }
    return result;
}
void PoseBank::setProgram(int state, const QByteArray &source) {
    const auto before = effective(state);
    const auto next = parseProgram(source);
    if (source.isEmpty())
        throw Error("An edited pose requires a counted program");
    if (before.source == source)
        return;
    // Earlier aliases still need the old program. Materialize just the last one;
    // all preceding empty slices continue to point to that preserved copy.
    auto candidate = *this;
    if (state > 0 && candidate.programs_[state - 1].source.isEmpty())
        candidate.programs_[state - 1] = before;
    candidate.programs_[state] = next;
    candidate.validateProgress(state);
    *this = std::move(candidate);
}
int PoseBank::appendProgram(const QByteArray &source) {
    if (programs_.size() >= 32768 || source.isEmpty())
        throw Error("A new pose needs a counted program within the signed state range");
    if (!programs_.isEmpty() && programs_.last().source.isEmpty())
        throw Error("Resolve the original trailing pose pointer before extending this table");
    auto candidate = *this;
    const int state = int(programs_.size());
    candidate.programs_ << parseProgram(source);
    candidate.validateProgress(state);
    *this = std::move(candidate);
    return state;
}
void PoseBank::validateProgress(int state) const {
    // Control flow is deterministic. Follow the authored entry, including what
    // executes after a delay; untouched unreachable instructions remain intact.
    QMap<quint64, int> seen;
    int current = state, index = 0, delays = 0;
    for (;;) {
        const auto &records = effective(current).records;
        if (index >= records.size()) return; // Native implicit END yields.
        const quint64 node = (quint64(current) << 32) | quint32(index);
        if (seen.contains(node)) {
            if (seen[node] == delays) throw Error("This edit creates a pose loop with no delay");
            return;
        }
        seen[node] = delays;
        const auto &record = records[index];
        if (!record.opcode || ((record.opcode == 1 || record.opcode == 21 || record.opcode == 3) &&
                              u8(record.operands, record.operands.size()-1) > 0)) ++delays;
        if (record.opcode == 4) index = u8(record.operands,0);
        else if (record.opcode == 5 || record.opcode == 22) {
            current = u8(record.operands, 0) + (record.opcode == 22 ? 256 * u8(record.operands, 1) : 0);
            index = 0;
        }
        else ++index;
    }
}
void PoseBank::setFrame(int state, int index, int token, int ticks) {
    auto records = effective(state).records;
    requireFrame(records, index);
    const auto next = frameRecord(token, ticks);
    // Retain an existing wide-token opcode, even for values below256.
    if (records[index].opcode == 21 && next.opcode == 1)
        records[index] = {21, next.operands.left(1) + QByteArray(1, 0) + next.operands.right(1)};
    else
        records[index] = next;
    setProgram(state, encodeProgram(records));
}
void PoseBank::insertFrame(int state, int before, int token, int ticks) {
    auto records = effective(state).records;
    const int count = int(records.size());
    if (before < 0 || before > count || count >= 255)
        throw Error("Frame insertion is outside the program or exceeds 255 instructions");
    const auto added = frameRecord(token, ticks);
    QVector<int> mapping;
    for (int i = 0; i < count; ++i)
        mapping << i + (i >= before);
    records.insert(before, added);
    remapJumps(records, mapping, count);
    setProgram(state, encodeProgram(records));
}
void PoseBank::removeFrame(int state, int index) {
    auto records = effective(state).records;
    requireFrame(records, index);
    const int count = int(records.size());
    QVector<int> mapping;
    for (int i = 0; i < count; ++i)
        mapping << i - (i > index);
    // A jump to the removed frame reaches the following instruction, or the end.
    records.removeAt(index);
    remapJumps(records, mapping, count);
    setProgram(state, encodeProgram(records));
}
void PoseBank::moveFrame(int state, int index, int destination) {
    auto records = effective(state).records;
    requireFrame(records, index);
    const int count = int(records.size());
    if (destination < 0 || destination >= count)
        throw Error("Frame destination is outside the program");
    if (index == destination)
        return;
    QVector<int> ids;
    for (int i = 0; i < count; ++i)
        ids << i;
    ids.move(index, destination);
    QVector<int> mapping(count);
    for (int i = 0; i < count; ++i)
        mapping[ids[i]] = i;
    records.move(index, destination);
    remapJumps(records, mapping, count);
    setProgram(state, encodeProgram(records));
}
void PoseBank::replaceFrames(int state, const QVector<QPair<int, int>> &frames) {
    if (frames.isEmpty())
        throw Error("A replacement sequence needs at least one frame");
    const auto before = effective(state).records;
    int last = -1;
    for (int i = 0; i < before.size(); ++i)
        if (frame(before[i])) last = i;
    if (last < 0)
        throw Error("Choose a pose containing frame instructions");
    QVector<PoseRecord> records;
    QVector<int> mapping(before.size());
    int next = 0;
    for (int i = 0; i < before.size(); ++i) {
        mapping[i] = int(records.size());
        if (!frame(before[i])) records << before[i];
        else if (next < frames.size()) {
            const auto [token, ticks] = frames[next++];
            records << frameRecord(token, ticks);
        }
        // Additional frames belong before the controls following the last frame.
        if (i == last)
            while (next < frames.size()) {
                const auto [token, ticks] = frames[next++];
                records << frameRecord(token, ticks);
            }
    }
    remapJumps(records, mapping, int(before.size()));
    setProgram(state, encodeProgram(records));
}
} // namespace lordly
