#include "engine/dialogues.h"
#include <QJsonArray>
#include <cmath>
namespace lordly {
namespace {
int integer(const QJsonValue &value, int low, int high) {
    const auto number = value.toDouble(-1);
    if (!value.isDouble() || !std::isfinite(number) || number != std::floor(number) ||
        number < low || number > high)
        throw Error("Invalid imported dialogue selector");
    return int(number);
}
QString text(const QJsonValue &value, int maximum, bool lines = false, bool controls = false) {
    if (!value.isString() || value.toString().isEmpty() || value.toString().size() > maximum)
        throw Error("Invalid imported dialogue text");
    const auto result = value.toString();
    for (const auto character : result) {
        const auto c = character.unicode();
        if (lines && (c == '\r' || c == '\n'))
            continue;
        if (c < 32 || c > 126 || (!controls && (c == '@' || c == '{' || c == '}')))
            throw Error("Imported dialogue contains unsupported characters or control tokens");
    }
    return result;
}
QByteArray authored(const QJsonObject &payload) {
    if (!payload["choice"].isNull() && !payload["choice"].isUndefined()) {
        if (!payload["choice"].isObject())
            throw Error("Invalid imported dialogue choice");
        const auto choice = payload["choice"].toObject();
        const auto first = text(choice["option1"], 24).trimmed(),
                   second = text(choice["option2"], 24).trimmed();
        if (first.isEmpty() || second.isEmpty())
            throw Error("Imported choice option is empty");
        return ("@S0@%0@n    1. {T05}" + first + "@n    2. {T05}" + second +
                "@1@w3001@c@n@2@w3255@c@n@#")
            .toLatin1();
    }
    const auto speaker = text(payload["speaker"], 28);
    auto body = text(payload["text"], 4096, true);
    body.replace("\r\n", "\n");
    body.replace('\r', '\n');
    QStringList lines, pages;
    for (auto paragraph : body.split('\n')) {
        auto rest = paragraph.trimmed();
        if (rest.isEmpty()) {
            lines << "";
            continue;
        }
        while (rest.size() > 30) {
            auto cut = rest.lastIndexOf(' ', 30);
            if (cut < 1)
                cut = 30;
            lines << rest.left(cut);
            rest = rest.mid(cut).trimmed();
        }
        lines << rest;
    }
    if (lines.size() > 24)
        throw Error("Imported dialogue exceeds eight text pages");
    for (int i = 0; i < lines.size(); i += 3)
        pages << lines.mid(i, 3).join("@n");
    return (speaker + "@n\"" + pages.join("@p@c") + "\"@a@s@c").toLatin1();
}
} // namespace
void Dialogues::importBrowser(const QJsonObject &cutscenes) {
    if (cutscenes["format"] != "ob64-cutscene-project" ||
        (cutscenes["schemaVersion"] != 1 && cutscenes["schemaVersion"] != 2) ||
        cutscenes["sourceRevision"] != "us-rev0" || !cutscenes["scenes"].isArray())
        throw Error("Unsupported browser cutscene project");
    struct Group {
        QMap<int, QByteArray> edits, added;
    };
    const auto keys = DialogueArchive::directory(rom_);
    QMap<quint32, Group> groups;
    auto rows = cutscenes["scenes"].toArray();
    if (cutscenes.contains("authoredScenes")) {
        if (!cutscenes["authoredScenes"].isArray())
            throw Error("Invalid imported authored scenes");
        for (const auto row : cutscenes["authoredScenes"].toArray())
            rows << row;
    }
    for (const auto row : rows) {
        if (!row.isObject())
            throw Error("Invalid imported scene row");
        const auto documentValue = row.toObject()["document"];
        if (documentValue.isNull())
            continue; // Unopened authored scene has no document edits.
        if (!documentValue.isObject())
            throw Error("Invalid imported scene document");
        const auto document = documentValue.toObject();
        if (!document["tracks"].isArray())
            throw Error("Invalid imported scene tracks");
        for (const auto track : document["tracks"].toArray()) {
            if (!track.isObject() || !track.toObject()["clips"].isArray())
                throw Error("Invalid imported scene track");
            for (const auto clip : track.toObject()["clips"].toArray()) {
                if (!clip.isObject() || !clip.toObject()["payload"].isObject())
                    throw Error("Invalid imported scene clip");
                const auto payload = clip.toObject()["payload"].toObject();
                const bool added = payload["nativeDialogueAuthored"].toBool();
                if (!added && (!payload["nativeDialogueEditable"].toBool() ||
                               payload["rawText"] == payload["originalRawText"]))
                    continue;
                if (clip.toObject()["kind"] != "dialogue")
                    throw Error("Imported dialogue edit belongs to a different clip kind");
                const int selector = integer(payload["presentationArchiveSelector"], 0,
                                             int(keys.size()) - 1),
                          index = integer(payload["presentationEntrySelector"], 0, 16383);
                const auto &archive = original(Table::Primary, selector);
                auto &group = groups[keys[selector]];
                const auto bytes = added ? authored(payload)
                                         : text(payload["rawText"], 65536, false, true).toLatin1();
                if (added) {
                    if (index < archive.entryCount() || group.added.contains(index))
                        throw Error("Imported new dialogue entry overlaps another entry");
                    group.added[index] = bytes;
                } else {
                    if (!payload["originalRawText"].isString() ||
                        QString::fromLatin1(archive.entry(index)) !=
                            payload["originalRawText"].toString())
                        throw Error("Imported dialogue original text differs from the loaded ROM");
                    if (group.edits.contains(index) && group.edits[index] != bytes)
                        throw Error("Imported scenes conflict on shared dialogue text");
                    group.edits[index] = bytes;
                }
            }
        }
    }
    auto next = *this;
    for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
        // The browser changes every selector referencing the same physical archive.
        for (int selector = 0; selector < keys.size(); ++selector) {
            if (keys[selector] != it.key())
                continue;
            auto changes = next.changes_.value(identity(Table::Primary, selector));
            const auto &archive = next.original(Table::Primary, selector);
            for (auto entry = it->edits.cbegin(); entry != it->edits.cend(); ++entry) {
                if (archive.entry(entry.key()) == entry.value())
                    changes.edits.remove(entry.key());
                else
                    changes.edits[entry.key()] = entry.value();
            }
            if (!it->added.isEmpty()) {
                const int needed = it->added.lastKey() - archive.entryCount() + 1;
                while (changes.added.size() < needed)
                    changes.added << QByteArray("@c");
                for (auto entry = it->added.cbegin(); entry != it->added.cend(); ++entry)
                    changes.added[entry.key() - archive.entryCount()] = entry.value();
            }
            archive.rebuild(changes.edits, changes.added);
            next.changes_[identity(Table::Primary, selector)] = std::move(changes);
        }
    }
    *this = std::move(next);
}
} // namespace lordly
