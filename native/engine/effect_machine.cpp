#include "engine/effect_machine.h"
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

namespace lordly {
EffectMachine::EffectMachine() : memory(8 * 1024 * 1024, 0) {}
quint32 EffectMachine::physical(quint32 a, quint32 size) const {
    const quint32 p = a & 0x1fffffff;
    if (p < 0x400 || quint64(p) + size > quint64(memory.size()))
        throw Error(
            QString("Effect memory reference outside its context: %1").arg(a, 8, 16, QChar('0')));
    return p;
}
void EffectMachine::mount(quint32 a, const QByteArray &data, bool executable) {
    const auto p = physical(a, quint32(data.size()));
    std::memcpy(memory.data() + p, data.constData(), data.size());
    if (executable)
        executable_.push_back({a, a + quint32(data.size())});
}
void EffectMachine::service(quint32 a, std::function<void()> callback) {
    services_.insert(a, std::move(callback));
}
void EffectMachine::observe(quint32 a, std::function<void()> callback) {
    observers_.insert(a, std::move(callback));
}
quint8 EffectMachine::byte(quint32 a) const {
    return quint8(memory.constData()[physical(a)]);
}
quint16 EffectMachine::half(quint32 a) const {
    const auto p = physical(a, 2);
    const auto *b = reinterpret_cast<const quint8 *>(memory.constData() + p);
    return (quint16(b[0]) << 8) | b[1];
}
quint32 EffectMachine::word(quint32 a) const {
    const auto p = physical(a, 4);
    const auto *b = reinterpret_cast<const quint8 *>(memory.constData() + p);
    return (quint32(b[0]) << 24) | (quint32(b[1]) << 16) | (quint32(b[2]) << 8) | b[3];
}
void EffectMachine::byte(quint32 a, quint8 v) {
    memory.data()[physical(a)] = char(v);
}
void EffectMachine::half(quint32 a, quint16 v) {
    const auto p = physical(a, 2);
    auto *b = memory.data() + p;
    b[0] = char(v >> 8);
    b[1] = char(v);
}
void EffectMachine::word(quint32 a, quint32 v) {
    const auto p = physical(a, 4);
    auto *b = memory.data() + p;
    for (int i = 0; i < 4; i++)
        b[i] = char(v >> (24 - i * 8));
}
float EffectMachine::single(int i) const {
    return std::bit_cast<float>(floating.at(i));
}
void EffectMachine::single(int i, float v) {
    floating.at(i) = std::bit_cast<quint32>(v);
}
double EffectMachine::real(int i) const {
    if (i & 1 || i > 30)
        throw Error("Unaligned effect double register");
    return std::bit_cast<double>((quint64(floating[i + 1]) << 32) | floating[i]);
}
void EffectMachine::real(int i, double v) {
    if (i & 1 || i > 30)
        throw Error("Unaligned effect double register");
    const auto bits = std::bit_cast<quint64>(v);
    floating[i] = quint32(bits);
    floating[i + 1] = quint32(bits >> 32);
}
float EffectMachine::number(quint32 a) const {
    return std::bit_cast<float>(word(a));
}
void EffectMachine::number(quint32 a, float v) {
    word(a, std::bit_cast<quint32>(v));
}
void EffectMachine::call(quint32 entry, quint32 budget) {
    constexpr quint32 returned = 0x80000100;
    registers[31] = returned;
    quint32 pc = entry, next = pc + 4;
    while (pc != returned) {
        pc_ = pc;
        if (!budget--)
            throw Error(QString("Effect instruction limit at %1").arg(pc, 8, 16, QChar('0')));
        if (auto observer = observers_.constFind(pc); observer != observers_.cend())
            (*observer)();
        if (auto service = services_.constFind(pc); service != services_.cend()) {
            (*service)();
            pc = registers[31];
            next = pc + 4;
            continue;
        }
        bool executable = false;
        for (const auto &[first, end] : executable_)
            if (pc >= first && quint64(pc) + 4 <= end) {
                executable = true;
                break;
            }
        if (!executable)
            throw Error(
                QString("Effect calls an unsupported service: %1").arg(pc, 8, 16, QChar('0')));
        try {
            auto advanced = step(pc, next);
            pc = advanced.first;
            next = advanced.second;
        } catch (const Error &e) {
            throw Error(QString::fromUtf8(e.what()) +
                        QString(" at %1").arg(pc_, 8, 16, QChar('0')));
        }
    }
}
std::pair<quint32, quint32> EffectMachine::step(quint32 pc, quint32 next) {
    const auto instruction = word(pc);
    const int op = instruction >> 26, rs = instruction >> 21 & 31, rt = instruction >> 16 & 31,
              rd = instruction >> 11 & 31;
    const int sa = instruction >> 6 & 31, fn = instruction & 63, imm = instruction & 65535;
    const auto simm = qint16(imm);
    auto &r = registers;
    const auto signedWord = [](quint32 v) { return std::bit_cast<qint32>(v); };
    const auto set = [&](int i, quint32 v) {
        if (i)
            r[i] = v;
    };
    bool branch = false, likely = false;
    quint32 target = 0;
    const auto jump = [&](quint32 a) {
        branch = true;
        target = a;
    };
    const auto when = [&](bool yes) {
        if (yes)
            jump(next + quint32(qint32(simm) * 4));
    };
    const quint32 address = r[rs] + quint32(qint32(simm));
    if (op == 17) {
        const int fs = rd, fd = sa;
        if (rs == 0)
            set(rt, floating[fs]);
        else if (rs == 4)
            floating[fs] = r[rt];
        else if (rs == 8) {
            likely = rt & 2;
            when(condition_ == bool(rt & 1));
        } else if (rs == 16 || rs == 17 || rs == 20) {
            const auto read = [&](int i) -> double {
                return rs == 16 ? single(i) : rs == 17 ? real(i) : signedWord(floating[i]);
            };
            const auto put = [&](double v) {
                if (rs == 16)
                    single(fd, float(v));
                else
                    real(fd, v);
            };
            const auto a = read(fs), b = read(rt);
            switch (fn) {
            case 0:
                put(a + b);
                break;
            case 1:
                put(a - b);
                break;
            case 2:
                put(a * b);
                break;
            case 3:
                if (b == 0)
                    throw Error("Effect divides by zero");
                put(a / b);
                break;
            case 4:
                put(std::sqrt(a));
                break;
            case 5:
                put(std::abs(a));
                break;
            case 6:
                put(a);
                break;
            case 7:
                put(-a);
                break;
            case 13:
                if (!std::isfinite(a) || a < -2147483648. || a >= 2147483648.)
                    throw Error("Invalid effect float conversion");
                floating[fd] = quint32(qint32(std::trunc(a)));
                break;
            case 32:
                single(fd, float(a));
                break;
            case 33:
                real(fd, a);
                break;
            case 50:
                condition_ = a == b;
                break;
            case 60:
                condition_ = a < b;
                break;
            case 62:
                condition_ = a <= b;
                break;
            default:
                throw Error(QString("Unsupported effect floating instruction %1").arg(fn));
            }
        } else
            throw Error(QString("Unsupported effect floating format %1").arg(rs));
    } else if (op == 49)
        floating[rt] = word(address);
    else if (op == 57)
        word(address, floating[rt]);
    else if (op == 53) {
        if (rt & 1 || rt > 30)
            throw Error("Unaligned effect double load");
        floating[rt + 1] = word(address);
        floating[rt] = word(address + 4);
    } else if (op == 61) {
        if (rt & 1 || rt > 30)
            throw Error("Unaligned effect double store");
        word(address, floating[rt + 1]);
        word(address + 4, floating[rt]);
    } else if (op == 34 || op == 38 || op == 42 || op == 46) {
        const bool left = op == 34 || op == 42, load = op == 34 || op == 38;
        const int n = address & 3, count = left ? 4 - n : n + 1;
        const auto start = left ? address : address & ~3U;
        quint32 value = r[rt];
        for (int i = 0; i < count; ++i) {
            const int shift = left ? 24 - i * 8 : (count - 1 - i) * 8;
            if (load)
                value = (value & ~(quint32(255) << shift)) | (quint32(byte(start + i)) << shift);
            else
                byte(start + i, quint8(value >> shift));
        }
        if (load)
            set(rt, value);
    } else
        switch (op) {
        case 0:
            switch (fn) {
            case 0:
                set(rd, r[rt] << sa);
                break;
            case 2:
                set(rd, r[rt] >> sa);
                break;
            case 3:
                set(rd, quint32(signedWord(r[rt]) >> sa));
                break;
            case 4:
                set(rd, r[rt] << (r[rs] & 31));
                break;
            case 6:
                set(rd, r[rt] >> (r[rs] & 31));
                break;
            case 7:
                set(rd, quint32(signedWord(r[rt]) >> (r[rs] & 31)));
                break;
            case 8:
                jump(r[rs]);
                break;
            case 9: {
                const auto destination = r[rs];
                set(rd, next + 4);
                jump(destination);
                break;
            }
            case 16:
                set(rd, hi_);
                break;
            case 17:
                hi_ = r[rs];
                break;
            case 18:
                set(rd, lo_);
                break;
            case 19:
                lo_ = r[rs];
                break;
            case 24: {
                const auto product = quint64(qint64(signedWord(r[rs])) * qint64(signedWord(r[rt])));
                lo_ = quint32(product);
                hi_ = quint32(product >> 32);
                break;
            }
            case 25: {
                const auto product = quint64(r[rs]) * r[rt];
                lo_ = quint32(product);
                hi_ = quint32(product >> 32);
                break;
            }
            case 26:
                if (r[rt]) {
                    const auto a = qint64(signedWord(r[rs])), b = qint64(signedWord(r[rt]));
                    lo_ = quint32(a / b);
                    hi_ = quint32(a % b);
                }
                break;
            case 27:
                if (r[rt]) {
                    lo_ = r[rs] / r[rt];
                    hi_ = r[rs] % r[rt];
                }
                break;
            case 32:
            case 33:
                set(rd, r[rs] + r[rt]);
                break;
            case 34:
            case 35:
                set(rd, r[rs] - r[rt]);
                break;
            case 36:
                set(rd, r[rs] & r[rt]);
                break;
            case 37:
                set(rd, r[rs] | r[rt]);
                break;
            case 38:
                set(rd, r[rs] ^ r[rt]);
                break;
            case 39:
                set(rd, ~(r[rs] | r[rt]));
                break;
            case 42:
                set(rd, signedWord(r[rs]) < signedWord(r[rt]));
                break;
            case 43:
                set(rd, r[rs] < r[rt]);
                break;
            default:
                throw Error(QString("Unsupported effect arithmetic instruction %1").arg(fn));
            }
            break;
        case 1:
            if (rt == 0)
                when(signedWord(r[rs]) < 0);
            else if (rt == 1)
                when(signedWord(r[rs]) >= 0);
            else if (rt == 2) {
                likely = true;
                when(signedWord(r[rs]) < 0);
            } else if (rt == 3) {
                likely = true;
                when(signedWord(r[rs]) >= 0);
            } else if (rt == 16 || rt == 17) {
                set(31, next + 4);
                when(rt == 16 ? signedWord(r[rs]) < 0 : signedWord(r[rs]) >= 0);
            } else
                throw Error("Unsupported effect conditional branch");
            break;
        case 2:
            jump((next & 0xf0000000) | ((instruction & 0x3ffffff) << 2));
            break;
        case 3:
            set(31, next + 4);
            jump((next & 0xf0000000) | ((instruction & 0x3ffffff) << 2));
            break;
        case 4:
            when(r[rs] == r[rt]);
            break;
        case 5:
            when(r[rs] != r[rt]);
            break;
        case 6:
            when(signedWord(r[rs]) <= 0);
            break;
        case 7:
            when(signedWord(r[rs]) > 0);
            break;
        case 20:
            likely = true;
            when(r[rs] == r[rt]);
            break;
        case 21:
            likely = true;
            when(r[rs] != r[rt]);
            break;
        case 22:
            likely = true;
            when(signedWord(r[rs]) <= 0);
            break;
        case 23:
            likely = true;
            when(signedWord(r[rs]) > 0);
            break;
        case 8:
        case 9:
            set(rt, address);
            break;
        case 10:
            set(rt, signedWord(r[rs]) < simm);
            break;
        case 11:
            set(rt, r[rs] < quint32(qint32(simm)));
            break;
        case 12:
            set(rt, r[rs] & imm);
            break;
        case 13:
            set(rt, r[rs] | imm);
            break;
        case 14:
            set(rt, r[rs] ^ imm);
            break;
        case 15:
            set(rt, quint32(imm) << 16);
            break;
        case 32:
            set(rt, quint32(qint32(qint8(byte(address)))));
            break;
        case 33:
            set(rt, quint32(qint32(qint16(half(address)))));
            break;
        case 35:
            set(rt, word(address));
            break;
        case 36:
            set(rt, byte(address));
            break;
        case 37:
            set(rt, half(address));
            break;
        case 40:
            byte(address, quint8(r[rt]));
            break;
        case 41:
            half(address, quint16(r[rt]));
            break;
        case 43:
            word(address, r[rt]);
            break;
        case 47:
            break; // Cache maintenance has no host-memory effect.
        default:
            throw Error(QString("Unsupported effect instruction %1").arg(op));
        }
    registers[0] = 0;
    if (branch)
        return {next, target};
    if (likely)
        return {next + 4, next + 8};
    return {next, next + 4};
}
} // namespace lordly
