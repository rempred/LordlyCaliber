#include "app/agent_disassembly.h"
#include <QStringList>
namespace lordly {
QString disassembleGameWord(quint32 pc, quint32 word, bool knownAddress) {
    static const QStringList regs =
        QStringLiteral("zero at v0 v1 a0 a1 a2 a3 t0 t1 t2 t3 t4 t5 t6 t7 s0 s1 s2 s3 s4 s5 s6 s7 "
                       "t8 t9 k0 k1 gp sp fp ra")
            .split(' ');
    const int op = word >> 26, rs = (word >> 21) & 31, rt = (word >> 16) & 31;
    const int rd = (word >> 11) & 31, sa = (word >> 6) & 31, fn = word & 63;
    const int imm = qint16(word);
    const auto hex = [](quint32 n) { return QString("0x%1").arg(n, 8, 16, QChar('0')); };
    const auto reg = [&](int i) { return "$" + regs[i]; };
    const auto fp = [](int i) { return "$f" + QString::number(i); };
    const int displacement = 4 + imm * 4;
    const auto target = knownAddress ? hex(pc + quint32(displacement))
                                     : QString(displacement < 0 ? ".%1" : ".+%1").arg(displacement);
    const QString raw = ".word " + hex(word);
    const auto ins = [](const QString &name, QStringList args) {
        return name + " " + args.join(", ");
    };
    const auto table = [](const char *ops, int index) {
        return QString::fromLatin1(ops).section(' ', index, index);
    };
    if (!word)
        return "nop";
    if (!op) {
        const auto name =
            table("sll - srl sra sllv - srlv srav jr jalr - - syscall break - sync mfhi mthi mflo "
                  "mtlo dsllv - dsrlv dsrav mult multu div divu dmult dmultu ddiv ddivu add addu "
                  "sub subu and or xor nor - - slt sltu dadd daddu dsub dsubu tge tgeu tlt tltu "
                  "teq - tne - dsll - dsrl dsra dsll32 - dsrl32 dsra32",
                  fn);
        if (name == "-")
            return raw;
        if (fn <= 3 || fn >= 56)
            return ins(name, {reg(rd), reg(rt), QString::number(sa)});
        if (fn <= 7 || (fn >= 20 && fn <= 23))
            return ins(name, {reg(rd), reg(rt), reg(rs)});
        if (fn == 8 || fn == 17 || fn == 19)
            return ins(name, {reg(rs)});
        if (fn == 9)
            return ins(name, {reg(rd), reg(rs)});
        if (fn == 12 || fn == 13)
            return ins(name, {QString::number((word >> 6) & 0xfffff)});
        if (fn == 15)
            return ins(name, {QString::number(sa)});
        if (fn == 16 || fn == 18)
            return ins(name, {reg(rd)});
        if (fn < 32)
            return ins(name, {reg(rs), reg(rt)});
        if (fn >= 48)
            return ins(name, {reg(rs), reg(rt), QString::number((word >> 6) & 1023)});
        return ins(name, {reg(rd), reg(rs), reg(rt)});
    }
    if (op == 1) {
        const auto name = table("bltz bgez bltzl bgezl - - - - tgei tgeiu tlti tltiu teqi - tnei - "
                                "bltzal bgezal bltzall bgezall",
                                rt);
        if (rt > 19 || name == "-")
            return raw;
        return ins(name, {reg(rs), rt >= 8 && rt <= 14 ? QString::number(imm) : target});
    }
    if (op == 2 || op == 3) {
        const auto destination = (word & 0x3ffffffu) << 2;
        return ins(op == 2 ? "j" : "jal",
                   {knownAddress ? hex(((pc + 4) & 0xf0000000u) | destination)
                                 : "((PC+4) & 0xf0000000) | " + hex(destination)});
    }
    if ((op >= 4 && op <= 7) || (op >= 20 && op <= 23)) {
        const auto name =
            table("beq bne blez bgtz beql bnel blezl bgtzl", op >= 20 ? op - 16 : op - 4);
        return (op & 2) ? ins(name, {reg(rs), target}) : ins(name, {reg(rs), reg(rt), target});
    }
    if (op >= 8 && op <= 15) {
        const auto name = table("addi addiu slti sltiu andi ori xori lui", op - 8);
        const auto value =
            op >= 12 ? QString("0x%1").arg(word & 65535, 0, 16) : QString::number(imm);
        return op == 15 ? ins(name, {reg(rt), value}) : ins(name, {reg(rt), reg(rs), value});
    }
    if (op == 24 || op == 25)
        return ins(op == 24 ? "daddi" : "daddiu", {reg(rt), reg(rs), QString::number(imm)});
    if (op == 16 || op == 17) {
        if (rs == 0 || rs == 1 || rs == 2 || rs == 4 || rs == 5 || rs == 6) {
            const auto move = table("mfc dmfc cfc - mtc dmtc ctc", rs) + QString::number(op - 16);
            const auto dst = op == 17 && rs != 2 && rs != 6 ? fp(rd) : "$" + QString::number(rd);
            return ins(move, {reg(rt), dst});
        }
        if (rs == 8 && rt <= 3)
            return ins(QString("bc%1").arg(op - 16) + table("f t fl tl", rt), {target});
        if (op == 16 && rs == 16) {
            if (fn == 24)
                return "eret";
            if (fn == 1)
                return "tlbr";
            if (fn == 2)
                return "tlbwi";
            if (fn == 6)
                return "tlbwr";
            if (fn == 8)
                return "tlbp";
        }
        if (op == 17 && (rs == 16 || rs == 17 || rs == 20 || rs == 21)) {
            const QString suffix = rs == 16 ? ".s" : rs == 17 ? ".d" : rs == 20 ? ".w" : ".l";
            if (fn < 16) {
                const auto name = table("add sub mul div sqrt abs mov neg round.l trunc.l ceil.l "
                                        "floor.l round.w trunc.w ceil.w floor.w",
                                        fn) +
                                  suffix;
                return fn < 4 ? ins(name, {fp(sa), fp(rd), fp(rt)}) : ins(name, {fp(sa), fp(rd)});
            }
            if (fn == 32 || fn == 33 || fn == 36 || fn == 37)
                return ins(QString("cvt.") + table("s d - - w l", fn - 32) + suffix,
                           {fp(sa), fp(rd)});
            if (fn >= 48)
                return ins("c." +
                               table("f un eq ueq olt ult ole ule sf ngle seq ngl lt nge le ngt",
                                     fn - 48) +
                               suffix,
                           {fp(rd), fp(rt)});
        }
        return raw;
    }
    if (op == 26 || op == 27 || op >= 32) {
        const auto name =
            op == 26   ? QString("ldl")
            : op == 27 ? QString("ldr")
                       : table("lb lh lwl lw lbu lhu lwr lwu sb sh swl sw sdl sdr swr cache ll "
                               "lwc1 lwc2 - lld ldc1 ldc2 ld sc swc1 swc2 - scd sdc1 sdc2 sd",
                               op - 32);
        if (name == "-")
            return raw;
        const auto dest = (op == 49 || op == 53 || op == 57 || op == 61) ? fp(rt)
                          : (op == 47 || op == 50 || op == 54 || op == 58 || op == 62)
                              ? QString::number(rt)
                              : reg(rt);
        return ins(name, {dest, QString("%1(%2)").arg(imm).arg(reg(rs))});
    }
    return raw;
}
} // namespace lordly
