#include "engine/scene_party.h"
#include "game/director/actor_body.h"
#include "game/director/actor_roster.h"
#include <QMap>
#include <QSet>
namespace lordly {
std::optional<SceneParty> SceneParty::infer(const DirectorProgram &program, int classCount) {
    SceneParty party;
    const int resource = program.resourceClass();
    party.preserved = resource == 2 || resource == 7;
    bool needed = party.preserved || resource == 8, hasRoster = false, special = false;
    int binding = -1, reset = -1;
    auto valid = [=](quint32 c) { return c > 0 && c < quint32(classCount); };
    auto add = [&](int c) {
        if (!valid(c) || party.classes.size() == 5)
            return;
        for (int old : party.classes)
            if (lordly_roster_same(quint8(old), quint8(c)))
                return;
        party.classes << c;
    };
    QSet<int> bound;
    for (int i = 0; i < program.commands.size(); ++i) {
        const auto &c = program.commands[i];
        const auto &a = c.operands;
        if (c.opcode == 0x92 && a.size() == 2)
            add(quint8(a[1]));
        if ((c.opcode == 0x45 || c.opcode == 0xab) && a.size() == 2) {
            hasRoster = needed = true;
            add(qint32(a[0]));
            add(qint32(a[1]));
        }
        if (c.opcode == 0xa6 && binding < 0)
            binding = i;
        if (c.opcode == 0x96 && reset < 0)
            reset = i;
        needed |= c.opcode == 0xa6 || c.opcode == 0x3f || c.opcode == 0xa4;
        special |= c.opcode == 0xa9;
        if ((c.opcode == 0x14 || c.opcode == 0x92 || c.opcode == 0x95 || c.opcode == 0xa6) &&
            !a.isEmpty())
            bound.insert(qint32(a[0]));
    }
    if (!needed)
        return {};
    if (party.classes.isEmpty() || !special)
        add(81);
    add(2);
    if (binding >= 0)
        for (int c : {4, 5, 13, 2, 4})
            if (party.classes.size() < 5 && valid(c))
                party.classes << c;
    party.deployed = binding >= 0 && (reset < 0 || binding < reset);
    QMap<int, int> speakers, members;
    auto portrait = [](const DirectorCommand &c) { return c.operands[10]; };
    if (hasRoster) {
        for (const auto &c : program.commands)
            if (c.opcode == 0xbf && c.operands.size() >= 11) {
                const int slot = qint32(c.operands[3]);
                if (slot >= 0 && slot < 28 && !bound.contains(slot) && valid(portrait(c)) &&
                    !speakers.contains(slot))
                    speakers.insert(slot, portrait(c));
            }
        QSet<int> walked;
        if (party.deployed)
            for (const auto &c : program.commands)
                if (c.opcode == 0xa6 && c.operands.size() == 2 && c.operands[1] < 5) {
                    const int member = c.operands[1];
                    walked.insert(member);
                    for (const auto &box : program.commands)
                        if (box.opcode == 0xbf && box.operands.size() >= 11 &&
                            box.operands[3] == c.operands[0] && valid(portrait(box))) {
                            if (!members.contains(member))
                                members.insert(member, portrait(box));
                            break;
                        }
                }
        auto values = speakers.values();
        for (int member = 0; member < 5 && !values.isEmpty(); ++member)
            if (!members.contains(member))
                members.insert(member, values.takeFirst());
        if (!members.isEmpty()) {
            int count = members.lastKey() + 1;
            for (int member : walked)
                count = qMax(count, member + 1);
            for (int member = 0; member < count; ++member)
                party.encounter << members.value(member, party.classes.value(member, 2));
            party.leaderOnly = walked.isEmpty() && count == 1;
        }
    }
    return party;
}
QString SceneParty::description() const {
    auto names = [](const QVector<int> &ids) {
        QStringList values;
        for (int id : ids)
            values << QString::number(id);
        return values.join(", ");
    };
    auto text = QString("Sample party classes: %1. Class-default equipment and a sample formation.")
                    .arg(names(classes));
    if (!encounter.isEmpty())
        text += QString(" Encounter classes inferred from dialogue: %1.").arg(names(encounter));
    return text;
}
void SceneParty::supply(SceneActors &actors, const BodyAnimationData &data, bool reset) const {
    ActorBindingInputs inputs;
    for (auto &row : inputs.rows)
        row = QByteArray(248, 0);
    QByteArray records;
    for (const auto &c : data.classes().entries())
        records += c.record;
    PresentationRoster roster;
    roster.sideGroups = {0, 30};
    int ordinal = 0;
    constexpr int formation[]{4, 1, 3, 5, 7};
    auto unit = [&](const QVector<int> &ids, bool side, int firstMember) {
        for (int member = 0; member < ids.size() && (!leaderOnly || member == 0); ++member) {
            if (ordinal == 20)
                throw Error("Preview party exceeds the native roster");
            const int primary = ids[member];
            const int secondaryDefault = u8(data.classes().entries().at(primary).record, 69);
            const int secondary = secondaryDefault ? secondaryDefault : primary;
            auto &row = inputs.rows[ordinal];
            put32(row, 0x40, (side ? 0x500 : 0) | (member == 0 ? 0x200 : 0));
            put32(row, 0x48, primary);
            put32(row, 0x4c, secondary);
            const int cell = formation[member], lane = side ? cell % 3 : 2 - cell % 3,
                      depth = side ? cell / 3 : 8 - cell / 3;
            put32(row, 0x54, lane);
            put32(row, 0x58, depth);
            put8(row, 0xf6, quint8(firstMember + member));
            uint16_t equipment[4];
            if (lordly_body_appearance(reinterpret_cast<const uint8_t *>(records.constData()),
                                       data.classes().entries().size(), primary, secondary,
                                       equipment))
                throw Error("Preview party has an invalid class appearance");
            for (int i = 0; i < 4; ++i)
                put16(row, 0x36 + i * 2, equipment[i]);
            const bool paired = secondary == 10 || secondary == 25 || secondary == 123 ||
                                data.routes().entry(primary).construction == 2;
            const int children = secondary == 1 ? 3 : paired ? 2 : 1;
            for (int child = 0; child < children; ++child) {
                const quint32 address = 0x80400000U + quint32(ordinal * 3 + child) * 0x100;
                put32(row, child * 4, address);
                QByteArray object(0x30, 0);
                const int dx = children == 3 ? (child == 2 ? 9 : -9) * (side ? -1 : 1) : 0;
                const int dz = children == 3 ? (child == 0 ? -9 : child == 1 ? 9 : 0) : 0;
                put16(object, 0x28, quint16(38 * (depth - 4) + dx));
                put16(object, 0x2c, quint16(38 * (lane - 1) + dz));
                actors.supplyBodyObject(address, object);
            }
            roster.members[side ? 1 : 0].insert(firstMember + member, {1, 0});
            if (side)
                inputs.unitMembers[member] = quint8(firstMember + member);
            ++ordinal;
        }
    };
    // Encounter rows precede the current unit. With no encounter, reset appends
    // a deployed copy after existing rows, retaining their Actor row identities.
    if (!encounter.isEmpty())
        unit(encounter, true, 6);
    unit(classes, false, 1);
    if (encounter.isEmpty() && (deployed || reset))
        unit(classes, true, 1);
    actors.supplyBindingInputs(std::move(inputs));
    actors.supplyRosterLaunch({-3, 0, roster});
    actors.supplyPresentationLaunch({0, -3, roster});
}
} // namespace lordly
