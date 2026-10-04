#include "engine/director.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace lordly {
namespace {
const QJsonObject &catalog() {
    static const QJsonObject value = [] {
        QFile file(":/game/director/grammar.json");
        if (!file.open(QIODevice::ReadOnly))
            throw Error("Missing Director grammar");
        const auto result = QJsonDocument::fromJson(file.readAll()).object();
        if (result.isEmpty())
            throw Error("Invalid Director grammar");
        return result;
    }();
    return value;
}
} // namespace
const QMap<quint32, DirectorOpcode> &Directors::grammar() {
    static const QMap<quint32, DirectorOpcode> result = [] {
        QMap<quint32, DirectorOpcode> rows;
        for (const auto &value : catalog()["rows"].toArray()) {
            const auto row = value.toObject();
            DirectorOpcode entry;
            entry.opcode = quint32(row["opcode"].toInteger());
            entry.words = row["words"].toInt();
            entry.name = row["name"].toString();
            entry.description = row["description"].toString();
            const auto query = row["query"].toString();
            entry.queryWords = query == "Q3" ? 3 : query == "Q4" ? 4 : 0;
            entry.requiredMode = row["mode"].toInt(-1);
            for (const auto &v : row["operands"].toArray())
                entry.operands << v.toString();
            if (entry.words < 1 || entry.words > 32 || entry.operands.size() != entry.words - 1 ||
                rows.contains(entry.opcode))
                throw Error("Invalid Director command width");
            rows.insert(entry.opcode, entry);
        }
        if (rows.size() != 154)
            throw Error("Incomplete Director grammar");
        return rows;
    }();
    return result;
}
DirectorProgram DirectorProgram::decode(const QByteArray &bytes) {
    if (bytes.isEmpty() || bytes.size() % 4 || bytes.size() > 0x1000000)
        throw Error("Director stream must contain bounded, complete words");
    DirectorProgram result;
    result.source = bytes;
    const auto &grammar = Directors::grammar();
    for (int at = 0; at < bytes.size();) {
        const auto opcode = u32(bytes, at);
        const auto definition = grammar.constFind(opcode);
        if (definition == grammar.end())
            throw Error(QString("Unknown Director opcode %1 at word %2")
                            .arg(opcode, 8, 16, QChar('0'))
                            .arg(at / 4));
        // Continuation programs can terminate without a resource-class trailer.
        const int width = opcode == 0x80000001 && at + 4 == bytes.size() ? 1 : definition->words;
        if (at + width * 4 > bytes.size())
            throw Error("Truncated Director operands");
        DirectorCommand command{at / 4, opcode, {}};
        for (int i = 1; i < width; ++i)
            command.operands << u32(bytes, at + i * 4);
        result.commands << command;
        at += width * 4;
    }
    return result;
}
QByteArray DirectorProgram::encode() const {
    QByteArray result;
    for (const auto &command : commands) {
        const auto at = result.size();
        result.resize(at + 4 + command.operands.size() * 4);
        put32(result, at, command.opcode);
        for (int i = 0; i < command.operands.size(); ++i)
            put32(result, at + 4 + i * 4, command.operands[i]);
    }
    const auto checked = decode(result);
    if (checked.commands.size() != commands.size())
        throw Error("Director edit changed command framing");
    for (int i = 0; i < commands.size(); ++i)
        if (checked.commands[i].opcode != commands[i].opcode ||
            checked.commands[i].operands != commands[i].operands)
            throw Error("Director edit changed operand framing");
    return result;
}
int DirectorProgram::resourceClass() const {
    if (commands.isEmpty())
        return -1;
    const auto &last = commands.last();
    if (last.opcode != 0x80000001 || last.operands.size() != 1 ||
        (last.operands[0] & 0xffffff00u) != 0xff000000u)
        return -1;
    return int(last.operands[0] & 255);
}
Directors Directors::load(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision() != 0)
        throw Error("Director resources currently require US revision 0");
    const auto &bytes = rom.bytes();
    constexpr quint32 table = 0x1f3ca84, base = 0x594280;
    const auto size = u32(bytes, table);
    if (!size || size % 4 || size > 65536 * 4 || quint64(table) + 4 + size > quint64(bytes.size()))
        throw Error("Invalid Director selector directory");
    const auto names = catalog()["names"].toObject();
    Directors result;
    qsizetype retained = 0;
    for (int i = 0; i < int(size / 4); ++i) {
        const auto key = u32(bytes, table + 4 + i * 4);
        result.selectors_ << key;
        if (!key)
            continue;
        if (!result.resources_.contains(key)) {
            if (key >= 0x10000000 || key % 2)
                throw Error("Invalid Director resource key");
            const quint32 entry = key + base, stored = u32(bytes, entry),
                          decoded = u32(bytes, entry + 4);
            if (stored < 4 || stored > 0x1000000 || !decoded || decoded > 0x1000000 ||
                quint64(entry) + 4 + stored > quint64(bytes.size()) ||
                (retained += decoded) > 128 * 1024 * 1024)
                throw Error("Director resource exceeds its envelope or memory budget");
            DirectorResource resource;
            resource.key = key;
            resource.envelope = entry;
            resource.program =
                DirectorProgram::decode(decodeLzss(bytes.mid(entry + 8, stored - 4), decoded));
            if (resource.program.resourceClass() < 0)
                throw Error("Directory-selected Director stream has no terminal resource class");
            const auto name = names[QString::number(key)].toObject();
            // A modified stream keeps its selector identity, but not a claimed
            // match to the recorded content. No saved payload is substituted.
            resource.name = name["name"].toString(QString("Director %1").arg(i));
            result.resources_.insert(key, std::move(resource));
        }
        result.resources_[key].selectors << i;
    }
    return result;
}
} // namespace lordly
