#include "engine/text.h"
#include "engine/class_text.h"
#include <QRegularExpression>

namespace lordly {
static const QByteArray lineBreak = QByteArray::fromHex("8197");
static const QByteArray reference = QByteArray::fromHex("8196");
QString decodeText(const QByteArray &bytes) {
    QString out;
    for (qsizetype pos = 0; pos < bytes.size(); ++pos) {
        auto c = u8(bytes, pos);
        if (c >= 32 && c <= 126)
            out += QChar(c);
        else if (c == 0x0e || c == 0x0f)
            continue;
        else if (c == 0x10 && pos + 1 < bytes.size()) {
            ++pos;
            out += '\n';
        } else if (bytes.mid(pos, 2) == lineBreak) {
            ++pos;
            out += '\n';
        } else if (bytes.mid(pos, 2) == reference) {
            pos += 2;
            QString digits;
            while (pos < bytes.size() && u8(bytes, pos) >= '0' && u8(bytes, pos) <= '9')
                digits += QChar(u8(bytes, pos++));
            --pos;
            if (digits.isEmpty())
                out += "{BYTE:81}{BYTE:96}";
            else
                out += "{REF:" + digits + "}";
        } else if (bytes.mid(pos, 2) == QByteArray::fromHex("876a")) {
            out += "{CTRL:876A}";
            ++pos;
        } else
            out += QString("{BYTE:%1}").arg(c, 2, 16, QChar('0')).toUpper();
    }
    return out;
}
static QByteArray encodeLine(const QString &text) {
    QByteArray out;
    const QRegularExpression marker("^\\{(BYTE|CTRL|SJIS|REF|HELP):([0-9A-Fa-f]+)\\}");
    for (qsizetype pos = 0; pos < text.size();) {
        if (text[pos] == '{') {
            auto match = marker.match(text.mid(pos));
            if (match.hasMatch()) {
                auto kind = match.captured(1), value = match.captured(2);
                if (kind == "REF" || kind == "HELP") {
                    bool ok = false;
                    auto number = value.toInt(&ok, 10);
                    if (!ok || !(number == 10 || (number >= 2800 && number <= 2822)))
                        throw Error("Unrecognized text reference");
                    out += reference + value.toLatin1();
                } else {
                    auto raw = QByteArray::fromHex(value.toLatin1());
                    if (raw.toHex() != value.toLower().toLatin1() ||
                        (kind == "BYTE" && raw.size() != 1) ||
                        (kind == "CTRL" && raw != QByteArray::fromHex("876a")))
                        throw Error("Invalid text byte marker");
                    out += raw;
                }
                pos += match.capturedLength();
                continue;
            }
            if (text.mid(pos).startsWith("{BYTE:") || text.mid(pos).startsWith("{REF:") ||
                text.mid(pos).startsWith("{CTRL:") || text.mid(pos).startsWith("{SJIS:") ||
                text.mid(pos).startsWith("{HELP:"))
                throw Error("Malformed text marker");
        }
        auto c = text[pos++].unicode();
        if (c < 32 || c > 126)
            throw Error("Use printable game text or explicit byte markers for encoded characters");
        out += char(c);
    }
    if (out.contains("\r\n"))
        throw Error("Description contains a record delimiter");
    return out;
}
QByteArray encodeText(const QString &text, bool paged) {
    auto lines = text.split('\n');
    if (lines.size() > 4)
        throw Error("Descriptions support at most four lines");
    QVector<QByteArray> encoded;
    for (const auto &line : lines)
        encoded << encodeLine(line);
    if (paged && encoded.size() > 2) {
        auto fourth = encoded.size() == 4 && !encoded[3].isEmpty() ? encoded[3] : QByteArray(" ");
        return QByteArray(1, 0x0e) + encoded[0] + QByteArray::fromHex("1030") + encoded[1] +
               QByteArray(1, 0x0f) + lineBreak + QByteArray(1, 0x0e) + encoded[2] +
               QByteArray::fromHex("1030") + fourth + QByteArray(1, 0x0f);
    }
    QByteArray out;
    for (int i = 0; i < encoded.size(); ++i) {
        if (i)
            out += lineBreak;
        out += encoded[i];
    }
    return out;
}
QMap<QString, TextBank> TextBank::load(const Rom &rom) {
    const QVector<TextSpec> specs = {
        {"classes", "Class descriptions", 0x353f92, 6399, 6408, 225, false},
        {"actions", "Action descriptions", 0x356180, 3145, 3154, 159, true},
        {"items", "Item descriptions", 0x34fa4e, 9702, 9710, 278, true},
        {"consumables", "Consumable descriptions", 0x352b88, 2055, 2064, 50, true}};
    QMap<QString, TextBank> result;
    const auto classText = rom.revision() == 0 ? ClassTextPlacement::read(rom) : std::nullopt;
    auto gap = rom.revision() == 0 ? 0x20248c2 : 0x2024516;
    for (const auto &spec : specs) {
        TextBank bank;
        bank.spec = spec;
        if (spec.id == "classes" && classText) {
            bank.resource = Resource::load(rom, classText->offset, classText->allocation);
            bank.spec.count = int(classText->count);
            bank.spec.capacity = int(classText->allocation) - 8;
            bank.spec.allocation = int(classText->allocation);
        } else {
            bank.resource = Resource::load(rom, gap + spec.relativeOffset, spec.allocation);
            bank.resource.capacity = spec.capacity;
        }
        qsizetype start = 0;
        while (start < bank.resource.decoded.size()) {
            auto end = bank.resource.decoded.indexOf("\r\n", start);
            if (end < 0)
                throw Error(spec.label + " contains an unterminated record");
            auto bytes = bank.resource.decoded.mid(start, end - start);
            bank.entries.push_back({bytes, bytes});
            start = end + 2;
        }
        if (bank.entries.size() != bank.spec.count)
            throw Error(spec.label + " has an unexpected record count");
        result[spec.id] = std::move(bank);
    }
    return result;
}
QString TextBank::text(int id) const {
    if (id < 0 || id >= entries.size())
        throw Error("Description index outside its resource");
    auto result = decodeText(entries[id].bytes);
    if (spec.id == "classes") {
        result.replace("{REF:", "{HELP:");
        if ((id >= 1 && id <= 164) || id >= 225) {
            auto suffix = result.indexOf("{HELP:");
            if (suffix >= 0)
                result = result.left(suffix);
        }
        if (id >= 200 && id <= 222 && result.startsWith('\n'))
            result.remove(0, 1);
    }
    if (spec.id == "actions")
        result.replace("{REF:10}", "Physical");
    while (result.endsWith('\n') || result.endsWith(' '))
        result.chop(1);
    return result;
}
void TextBank::setText(int id, const QString &text) {
    if (id < 0 || id >= entries.size())
        throw Error("Description index outside its resource");
    if (text == this->text(id))
        return;
    auto authored = text;
    if (spec.id == "actions" && entries[id].bytes.contains(reference + "10") &&
        !authored.contains("{REF:10}"))
        authored.replace("Physical", "{REF:10}");
    bool paged = spec.paged;
    if (spec.id == "actions" && !entries[id].bytes.contains(char(0x0e)) &&
        !entries[id].bytes.contains(char(0x0f)) && decodeText(entries[id].bytes).count('\n') >= 2)
        paged = false;
    auto encoded = encodeText(authored, paged);
    if (spec.id == "classes") {
        const auto &before = entries[id].bytes;
        auto ref = before.indexOf(reference);
        if (ref >= 0 && !encoded.contains(reference)) {
            auto begin = ref >= 2 && before.mid(ref - 2, 2) == lineBreak ? ref - 2 : ref;
            encoded += before.mid(begin);
        }
        if (id >= 200 && id <= 222 && before.startsWith(lineBreak) &&
            !encoded.startsWith(lineBreak))
            encoded.prepend(lineBreak);
        if (decodeText(encoded).count('\n') >= 4)
            throw Error("Description plus its retained help reference exceeds four lines");
    }
    entries[id].bytes = std::move(encoded);
}
QByteArray TextBank::serialized() const {
    QByteArray out;
    for (const auto &entry : entries)
        out += entry.bytes + "\r\n";
    return out;
}
bool TextBank::modified() const {
    if (entries.size() != spec.count)
        return true;
    for (const auto &entry : entries)
        if (entry.original != entry.bytes)
            return true;
    return false;
}
} // namespace lordly
