#include "engine/archive.h"
#include <QVector>
#include <algorithm>
#include <array>

namespace lordly {
namespace {
class Writer {
  public:
    void put(int width, quint32 value) {
        for (int bit = width - 1; bit >= 0; --bit) {
            pending_ = quint8((pending_ << 1) | ((value >> bit) & 1));
            if (++count_ == 8) {
                bytes_.append(char(pending_));
                count_ = 0;
                pending_ = 0;
            }
        }
    }
    QByteArray finish() {
        if (count_)
            bytes_.append(char(pending_ << (8 - count_)));
        return bytes_;
    }

  private:
    QByteArray bytes_;
    quint8 pending_ = 0;
    int count_ = 0;
};
struct Token {
    int symbol, distance;
};
void block(Writer &writer, const QVector<Token> &tokens) {
    writer.put(16, quint32(tokens.size()));
    // Complete canonical trees: character symbols 0/1 have eight bits and
    // 2..509 have nine. Their length alphabet contains only 10 and 11.
    // Fixed trees avoid mutable codec state and guarantee a bounded header.
    writer.put(5, 12);
    for (int i = 0; i < 12; ++i) {
        writer.put(3, i >= 10 ? 1 : 0);
        if (i == 2)
            writer.put(2, 0);
    }
    writer.put(9, 510);
    for (int i = 0; i < 510; ++i)
        writer.put(1, i >= 2 ? 1 : 0);
    // Position symbols 0/1 have three bits, 2..13 have four.
    writer.put(4, 14);
    for (int i = 0; i < 14; ++i)
        writer.put(3, i < 2 ? 3 : 4);
    for (const auto &token : tokens) {
        writer.put(token.symbol < 2 ? 8 : 9,
                   quint32(token.symbol < 2 ? token.symbol : token.symbol + 2));
        if (token.symbol >= 256) {
            const int position = token.distance - 1;
            int code = 0;
            for (int value = position; value; value >>= 1)
                ++code;
            writer.put(code < 2 ? 3 : 4, quint32(code < 2 ? code : code + 2));
            if (code > 1)
                writer.put(code - 1, quint32(position - (1 << (code - 1))));
        }
    }
}
void putLe16(QByteArray &bytes, qsizetype at, quint16 value) {
    put8(bytes, at, quint8(value));
    put8(bytes, at + 1, quint8(value >> 8));
}
void putLe32(QByteArray &bytes, qsizetype at, quint32 value) {
    putLe16(bytes, at, quint16(value));
    putLe16(bytes, at + 2, quint16(value >> 16));
}
int le16(const QByteArray &bytes, qsizetype at) {
    return u8(bytes, at) | (u8(bytes, at + 1) << 8);
}
} // namespace
QByteArray encodeLh5(const QByteArray &data) {
    if (data.size() > 0x1000000)
        throw Error("LH5 input exceeds 16 MiB");
    Writer writer;
    std::array<int, 4096> head;
    std::array<int, 8192> previous;
    head.fill(-1);
    previous.fill(-1);
    const auto *bytes = reinterpret_cast<const unsigned char *>(data.constData());
    const int size = int(data.size());
    auto hash = [&](int at) {
        return ((bytes[at] << 8) ^ (bytes[at + 1] << 4) ^ bytes[at + 2]) & 4095;
    };
    QVector<Token> tokens;
    tokens.reserve(32768);
    for (int at = 0; at < size;) {
        int length = 1, distance = 0;
        if (at + 2 < size) {
            int candidate = head[hash(at)], searched = 0;
            const int first = std::max(0, at - 8192), limit = std::min(256, size - at);
            while (candidate >= first && searched++ < 256) {
                int match = 0;
                while (match < limit && bytes[candidate + match] == bytes[at + match])
                    ++match;
                if (match >= 3 && match > length) {
                    length = match;
                    distance = at - candidate;
                    if (length == limit)
                        break;
                }
                candidate = previous[candidate & 8191];
            }
        }
        tokens << Token{distance ? length + 253 : bytes[at], distance};
        for (int i = 0; i < length && at + i + 2 < size; ++i) {
            const int position = at + i, key = hash(position);
            previous[position & 8191] = head[key];
            head[key] = position;
        }
        at += length;
        if (tokens.size() == 32768) {
            block(writer, tokens);
            tokens.clear();
        }
    }
    if (!tokens.isEmpty())
        block(writer, tokens);
    return writer.finish();
}
QByteArray LhaMember::rebuild(const Rom &rom, const QByteArray &data) const {
    // Authenticate the original framing before preserving its unknown metadata.
    decode(rom);
    const auto packed = encodeLh5(data);
    if (packed.size() > 0x1000000 || decodeLh5(packed, data.size()) != data)
        throw Error("Rebuilt LH5 payload exceeds its limit or failed readback");
    auto header = rom.bytes().mid(offset, headerSize);
    header.replace(2, 5, "-lh5-");
    const auto baseSize = level == 2 ? headerSize : u8(header, 0) + 2;
    putLe32(header, 7, quint32(packed.size() + (level == 1 ? headerSize - baseSize : 0)));
    putLe32(header, 11, quint32(data.size()));
    putLe16(header, level == 2 ? 21 : 22 + u8(header, 21), lhaCrc16(data));
    qsizetype crcAt = -1;
    if (level) {
        qsizetype at = level == 2 ? 24 : baseSize - 2;
        while (const auto size = le16(header, at)) {
            if (size < 3 || at + size + 2 > header.size())
                throw Error("Invalid original LHA extension");
            if (u8(header, at + 2) == 0) {
                if (size < 5 || crcAt >= 0)
                    throw Error("Invalid original LHA header CRC extension");
                crcAt = at + 3;
                putLe16(header, crcAt, 0);
            }
            at += size;
        }
    }
    if (level != 2) {
        quint8 sum = 0;
        for (qsizetype at = 2; at < baseSize; ++at)
            sum += u8(header, at);
        put8(header, 1, sum);
    }
    if (crcAt >= 0)
        putLe16(header, crcAt, lhaCrc16(header));
    return header + packed;
}
} // namespace lordly
