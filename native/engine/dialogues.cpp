#include "engine/dialogues.h"
#include <QJsonArray>
namespace lordly {
Dialogues Dialogues::load(Rom rom) {
    DialogueArchive::directory(rom);
    DialogueArchive::directory(rom, Table::Alternate);
    return Dialogues(std::move(rom));
}
quint32 Dialogues::identity(Table table, int archive) {
    if (archive < 0 || archive > 65535 || (table != Table::Primary && table != Table::Alternate))
        throw Error("Invalid dialogue archive identity");
    return quint32(archive) | (table == Table::Alternate ? 0x10000U : 0U);
}
const DialogueArchive &Dialogues::original(Table table, int archive) const {
    const auto key = identity(table, archive);
    if (!originals_.contains(key))
        originals_.insert(key, DialogueArchive::load(rom_, archive, table));
    return originals_.constFind(key).value();
}
int Dialogues::entryCount(Table table, int archive) const {
    return original(table, archive).entryCount() +
           int(changes_.value(identity(table, archive)).added.size());
}
QByteArray Dialogues::entry(Table table, int archive, int index) const {
    const auto &source = original(table, archive);
    const auto changes = changes_.value(identity(table, archive));
    if (index < 0 || index >= source.entryCount() + changes.added.size())
        throw Error("Dialogue entry is outside its archive");
    if (index >= source.entryCount())
        return changes.added[index - source.entryCount()];
    return changes.edits.contains(index) ? changes.edits[index] : source.entry(index);
}
void Dialogues::setEntry(Table table, int archive, int index, const QByteArray &text) {
    const auto &source = original(table, archive);
    const auto key = identity(table, archive);
    auto changes = changes_.value(key);
    if (index < 0 || index >= source.entryCount() + changes.added.size())
        throw Error("Dialogue entry is outside its archive");
    if (index >= source.entryCount())
        changes.added[index - source.entryCount()] = text;
    else if (source.entry(index) == text)
        changes.edits.remove(index);
    else
        changes.edits[index] = text;
    source.rebuild(changes.edits, changes.added);
    if (changes.edits.isEmpty() && changes.added.isEmpty())
        changes_.remove(key);
    else
        changes_[key] = std::move(changes);
}
int Dialogues::appendEntry(Table table, int archive, const QByteArray &text) {
    const auto &source = original(table, archive);
    const auto key = identity(table, archive);
    auto changes = changes_.value(key);
    const int index = source.entryCount() + int(changes.added.size());
    changes.added << text;
    source.rebuild(changes.edits, changes.added);
    changes_[key] = std::move(changes);
    return index;
}
void Dialogues::restoreEntry(Table table, int archive, int index) {
    setEntry(table, archive, index, original(table, archive).entry(index));
}
int Dialogues::changedEntries() const {
    int count = 0;
    for (const auto &change : changes_)
        count += int(change.edits.size() + change.added.size());
    return count;
}
QMap<quint32, QByteArray> Dialogues::plan() const {
    QMap<quint32, QByteArray> result;
    for (auto it = changes_.cbegin(); it != changes_.cend(); ++it)
        result[it.key()] =
            original(it.key() & 0x10000 ? Table::Alternate : Table::Primary, int(it.key() & 65535))
                .rebuild(it->edits, it->added);
    return result;
}
QJsonObject Dialogues::json() const {
    QJsonObject archives;
    for (auto it = changes_.cbegin(); it != changes_.cend(); ++it) {
        QJsonObject edits;
        for (auto entry = it->edits.cbegin(); entry != it->edits.cend(); ++entry)
            edits[QString::number(entry.key())] = QString::fromLatin1(entry->toHex());
        QJsonArray added;
        for (const auto &text : it->added)
            added.append(QString::fromLatin1(text.toHex()));
        archives[QString::number(it.key())] = QJsonObject{{"entries", edits}, {"added", added}};
    }
    return {{"version", 1}, {"archives", archives}};
}
void Dialogues::apply(const QJsonObject &value) {
    if (value["version"].toInt() != 1 || !value["archives"].isObject())
        throw Error("Invalid saved dialogue model");
    auto next = load(rom_);
    auto index = [](const QString &text, int maximum) {
        bool valid = false;
        const int result = text.toInt(&valid);
        if (!valid || result < 0 || result > maximum || QString::number(result) != text)
            throw Error("Invalid saved dialogue index");
        return result;
    };
    auto bytes = [](const QJsonValue &value) {
        const auto hex = value.toString().toLatin1(), result = QByteArray::fromHex(hex);
        if (!value.isString() || result.toHex() != hex.toLower())
            throw Error("Invalid saved dialogue bytes");
        return result;
    };
    const auto archives = value["archives"].toObject();
    for (auto it = archives.begin(); it != archives.end(); ++it) {
        const auto key = index(it.key(), 0x1ffff);
        const auto table = key & 0x10000 ? Table::Alternate : Table::Primary;
        if (!it->isObject())
            throw Error("Invalid saved dialogue archive");
        const auto change = it->toObject();
        if (!change["entries"].isObject() || !change["added"].isArray())
            throw Error("Invalid saved dialogue edits");
        const auto &source = next.original(table, key & 65535);
        Changes parsed;
        for (const auto &text : change["added"].toArray())
            parsed.added << bytes(text);
        const auto edits = change["entries"].toObject();
        for (auto entry = edits.begin(); entry != edits.end(); ++entry) {
            const int id = index(entry.key(), 16383);
            const auto text = bytes(entry.value());
            if (source.entry(id) != text)
                parsed.edits[id] = text;
        }
        source.rebuild(parsed.edits, parsed.added);
        if (!parsed.edits.isEmpty() || !parsed.added.isEmpty())
            next.changes_[quint32(key)] = std::move(parsed);
    }
    *this = std::move(next);
}
} // namespace lordly
