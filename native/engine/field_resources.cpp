#include "engine/field_resources.h"
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
    fieldChild(rom, root, index);
    const qsizetype at = (image.size() + 15) & ~qsizetype(15);
    if (at < 0x2800000 || at + 4 + payload.size() > 0x4000000)
        throw Error("Edited battlefield exceeds cartridge space");
    image += QByteArray(at - image.size() + 4, char(-1));
    put32(image, at, quint32(payload.size()));
    image += payload;
    while (image.size() % 16)
        image += char(-1);
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
