#include "engine/archive.h"
#include <QVector>
#include <array>
#include <optional>

namespace lordly {
namespace {
class Bits {
  public:
    explicit Bits(const QByteArray &data) : data_(data) {}
    quint32 get(int count) {
        if (count < 0 || count > 16 || position_ + count > data_.size() * 8)
            throw Error("Truncated LH5 bit stream");
        quint32 value = 0;
        for (int i = 0; i < count; ++i, ++position_)
            value = (value << 1) | ((u8(data_, position_ / 8) >> (7 - position_ % 8)) & 1);
        return value;
    }

  private:
    const QByteArray &data_;
    qsizetype position_ = 0;
};
// Canonical codes, with bounded depth. Separate trees make decoding reentrant;
// there is no shared module-level bit reader or stale tree state between blocks.
class Huffman {
  public:
    void single(int symbol, int alphabet) {
        if (symbol < 0 || symbol >= alphabet)
            throw Error("Invalid LH5 single-symbol tree");
        single_ = symbol;
    }
    void build(const QVector<int> &lengths) {
        single_ = -1;
        counts_.fill(0);
        for (const auto length : lengths) {
            if (length < 0 || length > 16)
                throw Error("LH5 code length exceeds 16 bits");
            if (length)
                ++counts_[length];
        }
        int code = 0, offset = 0;
        for (int length = 1; length <= 16; ++length) {
            code <<= 1;
            first_[length] = code;
            offsets_[length] = offset;
            code += counts_[length];
            offset += counts_[length];
            if (code > (1 << length))
                throw Error("Overlapping LH5 Huffman codes");
        }
        if (code != 65536)
            throw Error("Incomplete LH5 Huffman tree");
        symbols_.clear();
        for (int length = 1; length <= 16; ++length)
            for (int symbol = 0; symbol < lengths.size(); ++symbol)
                if (lengths[symbol] == length)
                    symbols_.push_back(symbol);
    }
    int decode(Bits &bits) const {
        if (single_ >= 0)
            return single_;
        int code = 0;
        for (int length = 1; length <= 16; ++length) {
            code = (code << 1) | int(bits.get(1));
            const auto index = code - first_[length];
            if (index >= 0 && index < counts_[length])
                return symbols_[offsets_[length] + index];
        }
        throw Error("Invalid LH5 Huffman code");
    }

  private:
    int single_ = -1;
    std::array<int, 17> counts_{}, first_{}, offsets_{};
    QVector<int> symbols_;
};
Huffman readLengths(Bits &bits, int alphabet, int width, int special) {
    Huffman tree;
    const int count = int(bits.get(width));
    if (!count) {
        tree.single(int(bits.get(width)), alphabet);
        return tree;
    }
    if (count > alphabet)
        throw Error("LH5 tree exceeds its alphabet");
    QVector<int> lengths(alphabet, 0);
    int i = 0;
    while (i < count) {
        int length = int(bits.get(3));
        if (length == 7)
            while (bits.get(1)) {
                if (++length > 16)
                    throw Error("LH5 code length exceeds 16 bits");
            }
        lengths[i++] = length;
        if (i == special) {
            i += int(bits.get(2));
            if (i > alphabet)
                throw Error("LH5 zero run exceeds its tree");
        }
    }
    tree.build(lengths);
    return tree;
}
Huffman readCharacters(Bits &bits, const Huffman &prefix) {
    Huffman tree;
    const int count = int(bits.get(9));
    if (!count) {
        tree.single(int(bits.get(9)), 510);
        return tree;
    }
    if (count > 510)
        throw Error("LH5 character tree exceeds its alphabet");
    QVector<int> lengths(510, 0);
    for (int i = 0; i < count;) {
        const auto symbol = prefix.decode(bits);
        if (symbol <= 2) {
            const auto run = symbol == 0   ? 1
                             : symbol == 1 ? int(bits.get(4)) + 3
                                           : int(bits.get(9)) + 20;
            if (i + run > 510)
                throw Error("LH5 zero run exceeds its character tree");
            i += run;
        } else {
            lengths[i++] = symbol - 2;
        }
    }
    tree.build(lengths);
    return tree;
}
quint16 le16(const QByteArray &data, qsizetype at) {
    return u8(data, at) | (quint16(u8(data, at + 1)) << 8);
}
quint32 le32(const QByteArray &data, qsizetype at) {
    return le16(data, at) | (quint32(le16(data, at + 2)) << 16);
}
} // namespace
quint16 lhaCrc16(const QByteArray &bytes) {
    quint16 crc = 0;
    for (const auto value : bytes) {
        crc ^= quint8(value);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xa001 : 0);
    }
    return crc;
}
QByteArray decodeLh5(const QByteArray &compressed, qsizetype size) {
    if (size < 0 || size > 0x1000000)
        throw Error("LH5 decoded size exceeds its limit");
    QByteArray output;
    output.reserve(size);
    Bits bits(compressed);
    std::array<quint8, 8192> dictionary{};
    int cursor = 0, remaining = 0;
    Huffman characters, positions;
    auto append = [&](quint8 value) {
        output.append(char(value));
        dictionary[cursor] = value;
        cursor = (cursor + 1) & 8191;
    };
    while (output.size() < size) {
        if (!remaining) {
            remaining = int(bits.get(16));
            if (!remaining)
                throw Error("LH5 block ended before its declared output size");
            const auto prefix = readLengths(bits, 19, 5, 3);
            characters = readCharacters(bits, prefix);
            positions = readLengths(bits, 14, 4, -1);
        }
        --remaining;
        const auto symbol = characters.decode(bits);
        if (symbol < 256) {
            append(quint8(symbol));
        } else {
            const auto length = symbol - 253;
            const auto code = positions.decode(bits);
            const auto distance =
                1 + (code < 2 ? code : (1 << (code - 1)) + int(bits.get(code - 1)));
            if (length > size - output.size())
                throw Error("LH5 match exceeds its declared output");
            for (int i = 0; i < length; ++i)
                append(dictionary[(cursor - distance + 8192) & 8191]);
        }
    }
    return output;
}
LhaMember LhaMember::read(const QByteArray &rom, qsizetype offset) {
    if (offset < 0 || offset + 24 > rom.size())
        throw Error("Truncated LHA header");
    LhaMember result;
    result.offset = offset;
    const auto method = rom.mid(offset + 2, 5);
    if (method != "-lh0-" && method != "-lh5-")
        throw Error("Unsupported LHA compression method");
    result.compressed = method == "-lh5-";
    result.storedSize = le32(rom, offset + 7);
    result.decodedSize = le32(rom, offset + 11);
    result.level = u8(rom, offset + 20);
    if (result.level > 2 || result.storedSize > 0x1000000 || result.decodedSize > 0x1000000)
        throw Error("Unsupported LHA member size or header level");
    result.headerSize = result.level == 2 ? le16(rom, offset) : u8(rom, offset) + 2;
    if (result.headerSize < (result.level == 2 ? 26 : 24) ||
        result.headerSize > rom.size() - offset)
        throw Error("LHA header exceeds its allocation");
    qsizetype extension = 0;
    if (result.level == 2) {
        result.checksum = le16(rom, offset + 21);
        extension = offset + 24;
    } else {
        const auto nameSize = u8(rom, offset + 21);
        if ((result.level == 1 ? 27 : 24) + nameSize > result.headerSize)
            throw Error("LHA filename exceeds its header");
        quint8 sum = 0;
        for (qsizetype i = 2; i < result.headerSize; ++i)
            sum += u8(rom, offset + i);
        if (sum != u8(rom, offset + 1))
            throw Error("LHA header checksum failed");
        result.name = QString::fromLatin1(rom.mid(offset + 22, nameSize));
        result.checksum = le16(rom, offset + 22 + nameSize);
        if (result.level == 1)
            extension = offset + result.headerSize - 2;
    }
    while (extension) {
        const auto size = le16(rom, extension);
        if (!size)
            break;
        if (size < 3 || extension + size + 2 > rom.size())
            throw Error("Invalid LHA extension");
        if (result.level == 1) {
            if (size > result.storedSize)
                throw Error("LHA extension exceeds packed size");
            result.headerSize += size;
            result.storedSize -= size;
        } else if (extension + size + 2 > offset + result.headerSize) {
            throw Error("LHA extension exceeds its header");
        }
        if (u8(rom, extension + 2) == 1)
            result.name = QString::fromLatin1(rom.mid(extension + 3, size - 3));
        extension += size;
    }
    if (result.name.isEmpty() || result.headerSize + result.storedSize > rom.size() - offset)
        throw Error("LHA member exceeds its ROM allocation");
    return result;
}
LhaMember LhaMember::find(const Rom &rom, const QString &name) {
    std::optional<LhaMember> found;
    qsizetype at = 0;
    while ((at = rom.bytes().indexOf("-lh", at)) >= 0) {
        const auto candidate = at - 2;
        at += 3;
        std::optional<LhaMember> member;
        try {
            member = read(rom.bytes(), candidate);
        } catch (const Error &) {
            continue;
        } // Signatures also occur inside compressed payloads.
        if (member->name != name)
            continue;
        if (found)
            throw Error("Duplicate named LHA member: " + name);
        found = member;
    }
    if (!found)
        throw Error("ROM is missing archive " + name);
    return *found;
}
QByteArray LhaMember::decode(const Rom &rom) const {
    const auto current = read(rom.bytes(), offset);
    if (current.headerSize != headerSize || current.storedSize != storedSize ||
        current.decodedSize != decodedSize || current.name != name ||
        current.checksum != checksum || current.compressed != compressed || current.level != level)
        throw Error("LHA member metadata changed since lookup");
    const auto data = rom.bytes().mid(offset + headerSize, storedSize);
    const auto decoded = compressed ? decodeLh5(data, decodedSize) : data;
    if (decoded.size() != decodedSize || lhaCrc16(decoded) != checksum)
        throw Error("LHA member failed its decoded size or CRC check: " + name);
    return decoded;
}
} // namespace lordly
