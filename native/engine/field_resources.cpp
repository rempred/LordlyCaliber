#include "engine/field_resources.h"
#include <QCryptographicHash>
namespace lordly {
namespace {
quint32 le32(const QByteArray &b, int at) {
    return u8(b, at) | (quint32(u8(b, at + 1)) << 8) | (quint32(u8(b, at + 2)) << 16) |
           (quint32(u8(b, at + 3)) << 24);
}
void putLe32(QByteArray &b, int at, quint32 n) {
    for (int i = 0; i < 4; ++i)
        put8(b, at + i, quint8(n >> (i * 8)));
}
QByteArray stored(const QByteArray &b) {
    QByteArray h(9, 0);
    h[0] = char(-128);
    putLe32(h, 1, quint32(b.size()));
    putLe32(h, 5, quint32(b.size()));
    return h + b;
}
} // namespace
QByteArray fieldResource(const Rom &rom, quint32 key) {
    const quint64 at = quint64(key) + 0x594280;
    if (key & 1 || at + 4 > quint64(rom.bytes().size()))
        throw Error("Invalid field resource key");
    const auto n = u32(rom.bytes(), qsizetype(at));
    if (!n || n > 8 * 1024 * 1024 || at + 4 + n > quint64(rom.bytes().size()))
        throw Error("Field resource exceeds ROM");
    return rom.bytes().mid(qsizetype(at + 4), n);
}
QVector<FieldMember> fieldMembers(const QByteArray &b) {
    QVector<FieldMember> out;
    int at = 0;
    while (at < b.size() - 1) {
        const int mode = u8(b, at);
        const auto size = le32(b, at + 1), decoded = le32(b, at + 5);
        if (out.size() >= 256 || (mode != 0x80 && mode != 0x85) || !size || !decoded ||
            decoded > 4 * 1024 * 1024 || quint64(at) + 9 + size > quint64(b.size() - 1))
            throw Error("Invalid compact field member");
        const auto data = b.mid(at + 9, size);
        if (mode == 0x80 && size != decoded)
            throw Error("Invalid stored field member length");
        out << FieldMember{b.mid(at, 9 + size), mode == 0x80 ? data : decodeLh5(data, decoded), at};
        at += 9 + size;
    }
    if (out.isEmpty() || at != b.size() - 1 || u8(b, at))
        throw Error("Invalid field collection terminator");
    return out;
}
QByteArray encodeFieldMembers(const QVector<FieldMember> &members) {
    QByteArray out;
    for (const auto &m : members) {
        bool same = false;
        if (!m.original.isEmpty()) {
            auto one = m.original + char(0);
            same = fieldMembers(one).first().data == m.data;
        }
        if (same)
            out += m.original;
        else {
            const auto packed = encodeLh5(m.data);
            if (packed.size() >= m.data.size())
                out += stored(m.data);
            else {
                QByteArray h(9, 0);
                h[0] = char(-123);
                putLe32(h, 1, quint32(packed.size()));
                putLe32(h, 5, quint32(m.data.size()));
                out += h + packed;
            }
        }
    }
    return out + char(0);
}
quint32 fieldChild(const Rom &rom, quint32 root, int index) {
    const auto b = fieldResource(rom, root);
    if (index < 0 || index >= b.size() / 4)
        throw Error("Field selector exceeds directory");
    return u32(b, index * 4);
}
void replaceFieldChild(QByteArray &image, quint32 root, int index, const QByteArray &payload) {
    const auto rom = Rom::fromBytes(image);
    const auto key = fieldChild(rom, root, index);
    const auto previous = fieldResource(rom, key);
    if (payload.isEmpty() || payload.size() > 8 * 1024 * 1024)
        throw Error("Invalid edited field resource size");
    if (previous == payload)
        return;
    // Only reuse a block carrying our complete ownership record. A resource
    // being outside retail ROM space alone does not establish ownership.
    constexpr qsizetype headerSize = 64;
    qsizetype header = qsizetype(key) + 0x594280 - headerSize;
    quint32 capacity = 0;
    const auto digest = [](const QByteArray &prefix, const QByteArray &data) {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(prefix); hash.addData(data);
        return hash.result();
    };
    if (header >= 0x2800000 && !(header & 15) &&
        image.mid(header, 8) == QByteArray("LCFIELD1", 8) &&
        u32(image, header + 8) == root && u32(image, header + 12) == quint32(index)) {
        const auto saved = u32(image, header + 16);
        if (saved >= 4 + previous.size() && saved <= 16 * 1024 * 1024 && !(saved & 15) &&
            header + headerSize + saved <= image.size() &&
            u32(image, header + 20) == previous.size() &&
            image.mid(header + 24, 32) == digest(image.mid(header, 24), previous) &&
            image.mid(header + 56, 8) == QByteArray(8, 0) &&
            image.mid(header + headerSize + 4 + previous.size(), saved - 4 - previous.size()) ==
                QByteArray(saved - 4 - previous.size(), char(-1)))
            capacity = saved;
    }
    quint32 wanted = 4096;
    while (wanted < 4 + payload.size()) wanted *= 2;
    if (!capacity || (wanted > capacity && header + headerSize + capacity != image.size())) {
        header = (image.size() + 15) & ~qsizetype(15);
        capacity = wanted;
    } else {
        capacity = std::max(capacity, wanted);
    }
    const qsizetype end = header + headerSize + capacity;
    if (header < 0x2800000 || end > 0x4000000)
        throw Error("Edited battlefield exceeds cartridge space");
    if (end > image.size()) image += QByteArray(end - image.size(), char(-1));
    QByteArray record(headerSize, 0);
    record.replace(0, 8, QByteArray("LCFIELD1", 8));
    put32(record, 8, root); put32(record, 12, quint32(index));
    put32(record, 16, capacity); put32(record, 20, quint32(payload.size()));
    record.replace(24, 32, digest(record.first(24), payload));
    image.replace(header, headerSize, record);
    const qsizetype at = header + headerSize;
    put32(image, at, quint32(payload.size()));
    image.replace(at + 4, payload.size(), payload);
    image.replace(at + 4 + payload.size(), capacity - 4 - payload.size(),
                  QByteArray(capacity - 4 - payload.size(), char(-1)));
    put32(image, root + 0x594284 + index * 4, quint32(at - 0x594280));
}
void replaceFieldMemberInPlace(QByteArray &image, quint32 key, int index, const QByteArray &data) {
    const auto members = fieldMembers(fieldResource(Rom::fromBytes(image), key));
    if (index < 0 || index >= members.size())
        throw Error("Invalid field placement selector");
    const auto &m = members[index];
    if (u8(m.original, 0) != 0x80 || m.data.size() != data.size())
        throw Error("This placement collection needs relocation support");
    image.replace(key + 0x594284 + m.offset + 9, data.size(), data);
}
} // namespace lordly
