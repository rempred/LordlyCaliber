#include "engine/event_reaction.h"
#include "engine/effect_machine.h"
#include "rebuild/accepted_rom.h"
#include <array>
namespace lordly {
namespace {
struct UnknownEventInput {};
}
std::optional<EventReaction> storyReaction(const QByteArray &bytes, int row, int group,
                                          int entry, int request, int answer,
                                          const QJsonObject &gates) {
    if (row <= 0 || group < 0 || group >= 6 || group * 2 + 2 > bytes.size() ||
        entry < 0 || request < entry || ((entry | request) & 1) || request + 2 > bytes.size() ||
        u8(bytes, request) != 0x10)
        return {};
    const int table = u16(bytes, group * 2);
    int slot = 0;
    while (table + 2 * slot + 2 <= bytes.size() && u16(bytes, table + 2 * slot) != entry) {
        if (u16(bytes, table + 2 * slot) == 0xffff || ++slot > 255)
            return {};
    }
    if (table + 2 * slot + 2 > bytes.size())
        return {};
    constexpr quint32 program = 0x80600000, context = 0x80197b00;
    EffectMachine m;
    m.mount(0x80170954, acceptedRomBytes(0x46854, 4672), true);
    m.mount(0x80190738, acceptedRomBytes(0x66638, 1308));
    m.mount(0x80171b94, acceptedRomBytes(0x47a94, 836), true);
    m.mount(0x80172124, acceptedRomBytes(0x48024, 280), true);
    m.mount(program, bytes);
    m.registers[29] = 0x807ff000;
    m.byte(context, group);
    m.byte(context + 1, slot);
    m.byte(context + 2, 1);
    m.byte(context + 3, 1); // Mission-specific phase after the common handlers.
    m.half(context + 4, (request - entry) / 2);
    m.half(context + 6, row);
    m.word(context + 12, 1u << group);
    m.word(context + 16, 1u << group);
    m.word(context + 24, program);
    m.byte(0x801936a8, row);
    m.service(0x80093380, [&] {
        if (m.registers[4] != 0x80196f60 || m.registers[5] != 512) throw UnknownEventInput{};
        m.mount(m.registers[4], QByteArray(512, 0));
    });
    EventReaction result;
    // Supply only three property callbacks. A new interpreter invocation has
    // a fresh local register frame, so reads of unset registers stay unresolved.
    constexpr quint32 dialogue = 0x80005000, flagRead = 0x80005004, flagWrite = 0x80005008;
    m.word(0x8018f1b0 + (0x90 >> 3) * 4, dialogue);
    m.word(0x8018f1b0 + (0x60 >> 3) * 4, flagRead);
    m.word(0x8018f220 + (0x60 >> 3) * 4, flagWrite);
    m.service(dialogue, [&] {
        if (m.registers[4] != 3) throw UnknownEventInput{};
        result.readAnswer = true;
        m.registers[2] = answer;
    });
    m.service(flagRead, [&] {
        const int key = qint16(m.registers[4]);
        const auto it = result.flags.constFind(std::abs(key));
        if (it == result.flags.cend()) throw UnknownEventInput{};
        m.registers[2] = key < 0 ? !it.value() : it.value();
    });
    m.service(flagWrite, [&] {
        const int flag = qint16(m.registers[4]);
        if (flag < 0 || flag >= 768) throw UnknownEventInput{};
        result.flags.insert(flag, m.registers[6] == 1);
    });
    // An empty local history permits the record write. Calendar values are
    // opaque: only the dated-history operation may consume them.
    m.mount(0x80197810, QByteArray(121 * 6, -1));
    enum Value { Unset, Known, Date };
    std::array<Value, 8> regs{};
    auto require = [&](int reg) { if (regs[reg] != Known) throw UnknownEventInput{}; };
    m.observe(0x80170954, [&] {
        if (m.registers[4] != program + entry) throw UnknownEventInput{};
    });
    m.observe(0x801709b0, [&] {
        const quint32 address = m.registers[2];
        if (address < program + entry || address + 2 > program + bytes.size() ||
            result.instructions.size() >= 256) throw UnknownEventInput{};
        const int pc = int(address - program);
        if (gates.contains(QString::number(pc))) throw UnknownEventInput{};
        const int op = u8(bytes, pc), arg = u8(bytes, pc + 1), r = op & 7;
        result.instructions.append(pc);
        if (op >= 0x68 && op <= 0x6f) {
            regs[r] = Known;
        } else if (op >= 0x88 && op <= 0x8f) {
            require(r);
        } else if (op >= 0x60 && op <= 0x67) {
            if (arg >= 0xf4 && arg <= 0xf6) {
                regs[r] = Date;
            } else if ((arg & 0xf8) == 0x90 || (arg & 0xf8) == 0x60 || arg < 8) {
                require(arg & 7);
                regs[r] = Known;
            } else throw UnknownEventInput{};
        } else if (op >= 0x70 && op <= 0x77 && (arg & 0xf8) == 0x60) {
            require(r);
            require(arg & 7);
        } else if (op == 0x15) {
            if (regs[0] != Date || regs[1] != Date || regs[2] != Date) throw UnknownEventInput{};
        } else if (op == 0x10 || op == 2) {
            require(0);
        } else if (op >= 0xa0 && ((op & 3) != 0 || (op & 0xc) == 0xc)) {
            require(op & 3);
            if ((op & 0xc) != 0xc) require((op & 0xc) >> 2);
        } else if (op != 1) {
            throw UnknownEventInput{};
        }
    });
    try {
        // The guest has written the raw selector and request at this boundary.
        // The subsequent death-scene classification affects transition flags.
        if (!m.callUntil(0x80172124, 0x80170ac0, 60000)) return {};
    } catch (const UnknownEventInput &) {
        return {};
    }
    result.requestOffset = result.instructions.last();
    result.selector = m.half(0x8018f1a0) & 0x3fff;
    return result;
}
}
