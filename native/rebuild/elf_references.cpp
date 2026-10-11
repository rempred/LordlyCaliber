#include "rebuild/elf_references.h"

namespace lordly {
QVector<CompiledAddressReference> readMipsAddressReferences(const QByteArray &object,
                                                            const CompiledModule &linked,
                                                            const QMap<QString, quint32> &symbols) {
    auto range = [&](quint64 offset, quint64 size) {
        if (offset > quint64(object.size()) || size > quint64(object.size()) - offset)
            throw Error("MIPS relocation data is outside its object file");
        return object.mid(qsizetype(offset), qsizetype(size));
    };
    if (range(0, 7) != QByteArray::fromHex("7f454c46010201") || u16(object, 16) != 1 ||
        u16(object, 18) != 8 || u16(object, 40) != 52 || u16(object, 46) != 40)
        throw Error("Expected a relocatable ELF32 big-endian MIPS object");
    const auto sectionCount = u16(object, 48);
    const auto headers = u32(object, 32);
    range(headers, quint64(sectionCount) * 40);
    auto section = [&](quint32 index) {
        if (index >= sectionCount)
            throw Error("Invalid relocation section index");
        return range(quint64(headers) + index * 40, 40);
    };
    auto contents = [&](const QByteArray &header) {
        return range(u32(header, 16), u32(header, 20));
    };
    auto label = [](const QByteArray &strings, quint32 offset) {
        const auto end = strings.indexOf(char(0), offset);
        if (offset >= quint32(strings.size()) || end < 0)
            throw Error("Invalid relocation symbol name");
        return QString::fromLatin1(strings.mid(offset, end - offset));
    };
    const auto names = contents(section(u16(object, 50)));
    int textIndex = -1;
    QByteArray text;
    for (int index = 0; index < sectionCount; ++index) {
        const auto header = section(index);
        if (label(names, u32(header, 0)) != ".text")
            continue;
        if (textIndex >= 0 || u32(header, 12) != 0)
            throw Error("Invalid object text section");
        textIndex = index;
        text = contents(header);
    }
    if (textIndex < 0)
        throw Error("Missing object text section");
    // The linker may insert alignment bytes before the input .text. Find that
    // displacement from actual function symbols, never from an assumed alignment.
    qint64 displacement = -1;
    for (int index = 0; index < sectionCount; ++index) {
        const auto header = section(index);
        if (u32(header, 4) != 2)
            continue;
        const auto table = contents(header), strings = contents(section(u32(header, 24)));
        if (u32(header, 36) != 16 || table.size() % 16)
            throw Error("Invalid object symbol table");
        for (qsizetype at = 0; at < table.size(); at += 16) {
            if (u8(table, at + 12) != 0x12 || u16(table, at + 14) != textIndex)
                continue;
            const auto name = label(strings, u32(table, at));
            if (!linked.entries.contains(name))
                throw Error("A compiled function is missing from the linked module");
            const qint64 delta = qint64(linked.entries[name]) - linked.address - u32(table, at + 4);
            if (delta < 0 || (delta & 3) || (displacement >= 0 && displacement != delta))
                throw Error("Object and linked function positions disagree");
            displacement = delta;
        }
    }
    if (displacement < 0 || displacement + text.size() > linked.bytes.size())
        throw Error("Object text does not fit its linked allocation");

    QVector<CompiledAddressReference> result;
    for (int index = 0; index < sectionCount; ++index) {
        const auto header = section(index);
        if (u32(header, 4) != 9 || u32(header, 28) != quint32(textIndex))
            continue;
        const auto relocations = contents(header), symbolHeader = section(u32(header, 24));
        const auto table = contents(symbolHeader),
                   strings = contents(section(u32(symbolHeader, 24)));
        if (u32(header, 36) != 8 || relocations.size() % 8 || u32(symbolHeader, 4) != 2 ||
            u32(symbolHeader, 36) != 16 || table.size() % 16)
            throw Error("Invalid MIPS address relocation table");
        QMap<quint32, QVector<quint32>> pending;
        for (qsizetype at = 0; at < relocations.size(); at += 8) {
            const auto offset = u32(relocations, at), info = u32(relocations, at + 4);
            const auto type = info & 255, symbolIndex = info >> 8;
            const quint64 symbolAt = quint64(symbolIndex) * 16;
            if (symbolAt + 16 > quint64(table.size()) || (offset & 3) ||
                offset + 4ULL > quint64(text.size()))
                throw Error("MIPS relocation points outside its symbol or text table");
            if (u16(table, symbolAt + 14) != 0 || (type != 5 && type != 6))
                continue; // Local references and call targets do not move with class data.
            if (type == 5) {
                pending[symbolIndex].append(offset);
                continue;
            }
            const auto highs = pending.take(symbolIndex);
            const auto name = label(strings, u32(table, symbolAt));
            if (highs.isEmpty() || !symbols.contains(name))
                throw Error("Unpaired or unbound MIPS data address: " + name);
            const auto low = u32(text, offset);
            const bool unsignedLow = low >> 26 == 13;
            const quint32 lowPart = unsignedLow ? (low & 65535) : quint32(qint16(low & 65535));
            for (const auto highAt : highs) {
                const auto high = u32(text, highAt);
                const quint32 addend = ((high & 65535) << 16) + lowPart;
                const quint32 hiSite = highAt + quint32(displacement),
                              loSite = offset + quint32(displacement);
                const auto placedHigh = u32(linked.bytes, hiSite),
                           placedLow = u32(linked.bytes, loSite);
                const quint32 actual =
                    ((placedHigh & 65535) << 16) +
                    (unsignedLow ? (placedLow & 65535) : quint32(qint16(placedLow & 65535)));
                if (high >> 26 != 15 || (placedHigh & 0xffff0000U) != (high & 0xffff0000U) ||
                    (placedLow & 0xffff0000U) != (low & 0xffff0000U) ||
                    actual != symbols[name] + addend)
                    throw Error("Linked instruction disagrees with its MIPS address relocation: " +
                                name);
                result.append({name, hiSite, loSite, qint32(addend)});
            }
        }
        if (!pending.isEmpty())
            throw Error("Unresolved high-half MIPS address relocation");
    }
    return result;
}
} // namespace lordly
