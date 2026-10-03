#include "engine/rom.h"
#include <QCryptographicHash>
#include <QFile>
#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <vector>

namespace lordly {
static void bounds(const QByteArray &data, qsizetype offset, qsizetype count) {
    if (offset < 0 || count < 0 || offset > data.size() || count > data.size() - offset)
        throw Error(QString("Data access outside resource at 0x%1 (%2 bytes)")
                        .arg(offset, 0, 16)
                        .arg(count));
}
quint8 u8(const QByteArray &d, qsizetype p) {
    bounds(d, p, 1);
    return static_cast<quint8>(d[p]);
}
quint16 u16(const QByteArray &d, qsizetype p) {
    return (quint16(u8(d, p)) << 8) | u8(d, p + 1);
}
quint32 u32(const QByteArray &d, qsizetype p) {
    return (quint32(u16(d, p)) << 16) | u16(d, p + 2);
}
void put8(QByteArray &d, qsizetype p, quint8 v) {
    bounds(d, p, 1);
    d[p] = char(v);
}
void put16(QByteArray &d, qsizetype p, quint16 v) {
    bounds(d, p, 2);
    put8(d, p, quint8(v >> 8));
    put8(d, p + 1, quint8(v));
}
void put32(QByteArray &d, qsizetype p, quint32 v) {
    bounds(d, p, 4);
    put16(d, p, quint16(v >> 16));
    put16(d, p + 2, quint16(v));
}
QByteArray sha256(const QByteArray &d) {
    return QCryptographicHash::hash(d, QCryptographicHash::Sha256).toHex();
}

QByteArray decodeLzss(const QByteArray &data, qsizetype size, qsizetype *consumed) {
    if (size < 0 || size > 64 * 1024 * 1024)
        throw Error("Invalid decoded resource size");
    QByteArray output;
    output.reserve(size);
    qsizetype pos = 0;
    auto next = [&]() { return u8(data, pos++); };
    auto repeat = [&](quint8 value, qsizetype count) {
        count = std::min(count, size - output.size());
        output.append(count, char(value));
    };
    auto copy = [&](qsizetype distance, qsizetype count) {
        count = std::min(count, size - output.size());
        for (qsizetype i = 0; i < count; ++i) {
            auto from = output.size() - distance;
            output.append(from < 0 ? char(0) : output[from]);
        }
    };
    while (output.size() < size) {
        auto b = next();
        if (b >= 0x80) {
            auto b2 = next();
            copy(((b & 7) << 8 | b2) + 1, ((b >> 3) & 15) + 3);
        } else if (b >= 0x40) {
            qsizetype count = (b & 63) + 1;
            bounds(data, pos, count);
            output.append(data.constData() + pos, std::min(count, size - output.size()));
            pos += count;
        } else if (b >= 0x20)
            repeat(0, (b & 31) + 2);
        else if (b >= 0x10) {
            auto b2 = next();
            auto b3 = next();
            copy(((b2 & 63) << 8 | b3) + 1, ((b & 15) | ((b2 & 192) >> 2)) + 4);
        } else if (b == 0) {
            auto n = next();
            auto hi = next();
            auto lo = next();
            copy((hi << 8 | lo) + 1, n + 5);
        } else if (b == 1 || b == 2) {
            auto n = next();
            repeat(b == 1 ? 255 : 0, n + 3);
        }
        // 0x03..0x0f are native no-ops. The input bound still guarantees termination.
    }
    if (consumed)
        *consumed = pos;
    return output;
}
QByteArray encodeLzss(const QByteArray &data) {
    if (data.size() > 16 * 1024 * 1024)
        throw Error("Resource exceeds the encoder limit");
    const int size = int(data.size());
    struct Match {
        int length[3] = {0, 0, 0};
        int distance[3] = {0, 0, 0};
    };
    struct Choice {
        int length = 0, distance = 0, kind = 0;
    };
    std::vector<Match> matches(size);
    std::vector<int> previous(size, -1), cost(size + 1, 0);
    std::array<int, 65536> heads;
    heads.fill(-1);
    std::vector<Choice> choices(size);
    auto byte = [&](int pos) { return static_cast<quint8>(data[pos]); };
    for (int pos = 0; pos + 2 < size; ++pos) {
        auto hash = ((quint32(byte(pos)) * 251 + byte(pos + 1)) * 251 + byte(pos + 2)) & 65535;
        int candidate = heads[hash];
        previous[pos] = candidate;
        heads[hash] = pos;
        const int maximum = std::min(260, size - pos);
        while (candidate >= 0 && pos - candidate <= 65536) {
            int distance = pos - candidate, length = 0;
            while (length < maximum && data[pos + length] == data[pos + length - distance])
                ++length;
            for (int type = 0; type < 3; ++type) {
                const int minimum = type + 3;
                const int limit = type == 0 ? 18 : type == 1 ? 67 : 260;
                const int range = type == 0 ? 2048 : type == 1 ? 16384 : 65536;
                if (distance <= range && length >= minimum &&
                    std::min(length, limit) > matches[pos].length[type]) {
                    matches[pos].length[type] = std::min(length, limit);
                    matches[pos].distance[type] = distance;
                }
            }
            if (matches[pos].length[0] == std::min(18, maximum) &&
                (maximum < 4 || matches[pos].length[1] == std::min(67, maximum)) &&
                (maximum < 5 || matches[pos].length[2] == maximum))
                break;
            candidate = previous[candidate];
        }
    }
    int fillRun = 0;
    for (int pos = size - 1; pos >= 0; --pos) {
        int best = std::numeric_limits<int>::max();
        auto consider = [&](int kind, int length, int distance, int bytes) {
            int total = bytes + cost[pos + length];
            if (total < best || (total == best && length > choices[pos].length)) {
                best = total;
                choices[pos] = {length, distance, kind};
            }
        };
        for (int n = 1; n <= 64 && pos + n <= size; ++n)
            consider(0, n, 0, n + 1);
        if ((byte(pos) == 0 || byte(pos) == 255) && pos + 1 < size && data[pos] == data[pos + 1])
            fillRun = std::min(258, fillRun + 1);
        else
            fillRun = 1;
        if (byte(pos) == 0)
            for (int n = 2; n <= std::min(33, fillRun); ++n)
                consider(1, n, 0, 1);
        if (byte(pos) == 0 || byte(pos) == 255)
            for (int n = 3; n <= fillRun; ++n)
                consider(byte(pos) == 0 ? 2 : 3, n, 0, 2);
        for (int type = 0; type < 3; ++type)
            for (int n = type + 3; n <= matches[pos].length[type]; ++n)
                consider(4 + type, n, matches[pos].distance[type], 2 + type);
        cost[pos] = best;
    }
    QByteArray out;
    out.reserve(cost[0]);
    auto emitByte = [&](int value) { out.append(char(value)); };
    for (int pos = 0; pos < size;) {
        auto c = choices[pos];
        int distance = c.distance - 1;
        switch (c.kind) {
        case 0:
            emitByte(0x40 + c.length - 1);
            out.append(data.constData() + pos, c.length);
            break;
        case 1:
            emitByte(0x20 + c.length - 2);
            break;
        case 2:
        case 3:
            emitByte(c.kind == 2 ? 2 : 1);
            emitByte(c.length - 3);
            break;
        case 4:
            emitByte(0x80 | ((c.length - 3) << 3) | (distance >> 8));
            emitByte(distance);
            break;
        case 5:
            emitByte(0x10 | ((c.length - 4) & 15));
            emitByte(((c.length - 4) & 48) << 2 | (distance >> 8));
            emitByte(distance);
            break;
        case 6:
            emitByte(0);
            emitByte(c.length - 5);
            emitByte(distance >> 8);
            emitByte(distance);
            break;
        }
        pos += c.length;
    }
    return out;
}
Rom Rom::fromBytes(QByteArray bytes, const QString &sourcePath) {
    if (bytes.size() < 64 || bytes.size() % 4)
        throw Error("ROM must contain a complete N64 header and whole 32-bit words");
    Rom rom;
    rom.path_ = sourcePath;
    switch (u32(bytes, 0)) {
    case 0x80371240:
        rom.order_ = ByteOrder::BigEndian;
        break;
    case 0x37804012:
        rom.order_ = ByteOrder::ByteSwapped;
        for (qsizetype i = 0; i < bytes.size(); i += 2)
            std::swap(bytes[i], bytes[i + 1]);
        break;
    case 0x40123780:
        rom.order_ = ByteOrder::LittleEndian;
        for (qsizetype i = 0; i < bytes.size(); i += 4) {
            std::swap(bytes[i], bytes[i + 3]);
            std::swap(bytes[i + 1], bytes[i + 2]);
        }
        break;
    default:
        throw Error("Unrecognized N64 ROM byte order");
    }
    rom.bytes_ = std::move(bytes);
    return rom;
}
Rom Rom::open(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw Error(file.errorString());
    if (file.size() > 128 * 1024 * 1024)
        throw Error("ROM exceeds the supported 128 MiB file limit");
    auto bytes = file.readAll();
    if (bytes.size() != file.size())
        throw Error("Incomplete ROM read");
    return fromBytes(std::move(bytes), file.fileName());
}
QString Rom::extension() const {
    return order_ == ByteOrder::ByteSwapped    ? "v64"
           : order_ == ByteOrder::LittleEndian ? "n64"
                                               : "z64";
}
QString Rom::title() const {
    return QString::fromLatin1(bytes_.mid(0x20, 20)).split(QChar(0)).first().trimmed();
}
int Rom::revision() const {
    return u8(bytes_, 0x3f);
}
void Rom::requireSupported() const {
    if (bytes_.size() < 0x2800000 || title() != "OgreBattle64" || bytes_.mid(0x3b, 2) != "NO" ||
        u8(bytes_, 0x3e) != 'E' || revision() > 1)
        throw Error("This editor currently supports US Ogre Battle 64 revisions 0 and 1");
}
QByteArray Rom::serialize(const QByteArray &normalized) const {
    if (normalized.size() % 4)
        throw Error("Export is not word aligned");
    QByteArray out = normalized;
    if (order_ == ByteOrder::ByteSwapped)
        for (qsizetype i = 0; i < out.size(); i += 2)
            std::swap(out[i], out[i + 1]);
    if (order_ == ByteOrder::LittleEndian)
        for (qsizetype i = 0; i < out.size(); i += 4) {
            std::swap(out[i], out[i + 3]);
            std::swap(out[i + 1], out[i + 2]);
        }
    return out;
}
void Rom::updateChecksum(QByteArray &data) {
    bounds(data, 0x1000, 0x100000);
    quint32 t1 = 0xf8ca4ddc, t2 = t1, t3 = t1, t4 = t1, t5 = t1, t6 = t1;
    for (qsizetype i = 0x1000; i < 0x101000; i += 4) {
        auto d = u32(data, i);
        auto sum = t6 + d;
        if (sum < t6)
            ++t4;
        t6 = sum;
        t3 ^= d;
        auto r = std::rotl(d, int(d & 31));
        t5 += r;
        t2 ^= t2 > d ? r : t6 ^ d;
        t1 += t5 ^ d;
    }
    put32(data, 0x10, t6 ^ t4 ^ t3);
    put32(data, 0x14, t5 ^ t2 ^ t1);
}
} // namespace lordly
