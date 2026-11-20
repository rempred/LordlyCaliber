#include "engine/agent_activity.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QSet>
#include <QUuid>

namespace lordly {
namespace {
QString digest(QJsonValue value) {
    if (value.isUndefined())
        return {};
    return QString::fromLatin1(
        sha256(QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact)));
}
QString scopeTitle(QString key) {
    QString title;
    for (auto c : key) {
        if (c.isUpper())
            title += ' ';
        title += c.toLower();
    }
    if (!title.isEmpty())
        title[0] = title[0].toUpper();
    return title;
}
const QList<Table> tables{Table::Classes, Table::Items, Table::Consumables};
QMap<QString, QJsonObject> patches(const Project &p) {
    QMap<QString, QJsonObject> out;
    for (const auto &v : p.codePatches())
        out.insert(v.toObject()["id"].toString(), v.toObject());
    return out;
}
QStringList keysTogether(const QJsonObject &a, const QJsonObject &b) {
    auto keys = a.keys() + b.keys();
    keys.removeDuplicates();
    keys.sort();
    return keys;
}
QJsonObject state(const Project &p) {
    auto obj = p.manifest();
    for (const auto key : {"agentActivity", "sourcePath", "sourceSha256", "version", "format",
                           "codePatches", "records"})
        obj.remove(key);
    return obj;
}
QString sectionResult(const QJsonValue &value) {
    return value.isUndefined() ? QString("Restored the source setting")
                               : QString("Updated project data");
}
QString currentDigest(const Project &p, const QJsonObject &sc, const QJsonObject &sections,
                      const QMap<QString, QJsonObject> &patchMap) {
    const auto kind = sc["kind"].toString(), key = sc["key"].toString();
    if (kind == "section")
        return digest(sections.value(key));
    if (kind == "patch")
        return patchMap.contains(key) ? digest(patchMap.value(key)) : QString();
    if (kind == "text") {
        const int row = sc["row"].toInt(-1);
        auto bank = p.textBanks().constFind(key);
        return bank != p.textBanks().cend() && row >= 0 && row < bank->entries.size()
                   ? digest(bank->text(row))
                   : QString();
    }
    for (auto table : tables)
        if (sc["table"] == tableKey(table)) {
            const int row = sc["row"].toInt(-1);
            if (row < 0 || row >= p.records(table).size())
                return {};
            if (kind == "name")
                return digest(p.record(table, row).name);
            if (kind == "record")
                return QString::fromLatin1(sha256(p.record(table, row).bytes));
            if (kind == "field") {
                const auto &columns = fields(table);
                for (int i = 0; i < columns.size(); ++i)
                    if (columns[i].key == key)
                        return digest(p.value(table, row, i));
            }
        }
    return {};
}
} // namespace

void validateAgentActivity(const QJsonValue &value) {
    if (!value.isArray())
        throw Error("Project CLI activity must be an array");
    QSet<QString> ids;
    for (const auto &v : value.toArray()) {
        const auto batch = v.toObject();
        const auto id = batch["id"].toString();
        if (id.isEmpty() || ids.contains(id) || !batch["edits"].isArray() ||
            !batch["source"].isString() ||
            !QDateTime::fromString(batch["time"].toString(), Qt::ISODateWithMs).isValid())
            throw Error("Invalid saved CLI activity batch");
        ids.insert(id);
        for (const auto &entry : batch["edits"].toArray()) {
            const auto row = entry.toObject();
            if (!row["subject"].isString() || !row["description"].isString() ||
                !row["scope"].isObject() || !row["before"].isString() || !row["after"].isString())
                throw Error("Invalid saved CLI edit");
        }
    }
}

void recordAgentEdits(const Project &before, Project &after, const QString &source,
                      const QStringList &actions) {
    QJsonArray edits;
    auto add = [&](QJsonObject scope, const QString &subject, const QString &description,
                   const QString &from, const QString &to) {
        edits.append(QJsonObject{{"scope", scope},
                                 {"subject", subject},
                                 {"description", description},
                                 {"before", from},
                                 {"after", to}});
    };
    for (auto table : tables) {
        const auto &old = before.records(table), &now = after.records(table);
        const auto &columns = fields(table);
        for (int r = std::min(old.size(), now.size()); r < std::max(old.size(), now.size()); ++r) {
            const bool created = r < now.size();
            const auto &rec = created ? now[r] : old[r];
            const auto fingerprint = QString::fromLatin1(sha256(rec.bytes));
            add({{"kind", "record"}, {"table", tableKey(table)}, {"row", r}},
                QString("%1 · %2 (ID %3)").arg(scopeTitle(tableKey(table)), rec.name).arg(r),
                created ? "Created record" : "Removed record", created ? QString() : fingerprint,
                created ? fingerprint : QString());
        }
        for (int r = 0; r < std::min(old.size(), now.size()); ++r) {
            if (old[r].name != now[r].name) {
                add({{"kind", "name"}, {"table", tableKey(table)}, {"row", r}},
                    QString("%1 · ID %2").arg(scopeTitle(tableKey(table))).arg(r),
                    old[r].name + " → " + now[r].name, digest(old[r].name), digest(now[r].name));
            }
            if (old[r].bytes == now[r].bytes)
                continue;
            QJsonObject scope{{"kind", "field"}, {"table", tableKey(table)}, {"row", r}};
            const auto subject =
                QString("%1 · %2 (ID %3)").arg(scopeTitle(tableKey(table)), now[r].name).arg(r);
            bool named = false;
            for (int col = 0; col < columns.size(); ++col) {
                const int a = before.value(table, r, col), b = after.value(table, r, col);
                if (a == b)
                    continue;
                named = true;
                scope["key"] = columns[col].key;
                add(scope, subject, QString("%1: %2 → %3").arg(columns[col].label).arg(a).arg(b),
                    digest(a), digest(b));
            }
            if (!named) {
                scope["kind"] = "record";
                add(scope, subject, "Updated native record bytes",
                    QString::fromLatin1(sha256(old[r].bytes)),
                    QString::fromLatin1(sha256(now[r].bytes)));
            }
        }
    }
    for (auto it = after.textBanks().cbegin(); it != after.textBanks().cend(); ++it) {
        auto old = before.textBanks().constFind(it.key());
        if (old == before.textBanks().cend())
            continue;
        for (int row = 0; row < std::min(it->entries.size(), old->entries.size()); ++row) {
            if (it->entries[row].bytes == old->entries[row].bytes)
                continue;
            const auto a = old->text(row), b = it->text(row);
            add({{"kind", "text"}, {"key", it.key()}, {"row", row}},
                QString("%1 text · ID %2").arg(scopeTitle(it.key())).arg(row),
                a.left(160) + " → " + b.left(160), digest(a), digest(b));
        }
    }
    const auto oldPatches = patches(before), newPatches = patches(after);
    auto ids = oldPatches.keys() + newPatches.keys();
    ids.removeDuplicates();
    ids.sort();
    for (const auto &id : ids) {
        const auto a = oldPatches.value(id), b = newPatches.value(id);
        if (a == b)
            continue;
        const auto p = b.isEmpty() ? a : b;
        QString detail;
        if (b.isEmpty())
            detail = "Removed";
        else if (a.isEmpty())
            detail = "Added";
        else if (a["source"] != b["source"])
            detail = "Revised source for";
        else if (a["enabled"].toBool(true) != b["enabled"].toBool(true))
            detail = b["enabled"].toBool(true) ? "Enabled" : "Disabled";
        else
            detail = "Updated";
        const auto lang = p["language"] == "asm" ? QString("assembly") : QString("C");
        const auto target = p.contains("module") ? p["module"].toString()
                            : p.contains("owner")
                                ? p["owner"].toString()
                                : QString("ROM 0x%1").arg(p["offset"].toInteger(), 0, 16);
        add({{"kind", "patch"}, {"key", id}}, id,
            QString("%1 %2 patch · %3").arg(detail, lang, target),
            a.isEmpty() ? QString() : digest(a), b.isEmpty() ? QString() : digest(b));
    }
    const auto a = state(before), b = state(after);
    QStringList sections;
    for (const auto &key : keysTogether(a, b)) {
        if (a.value(key) == b.value(key))
            continue;
        sections << key;
        add({{"kind", "section"}, {"key", key}}, scopeTitle(key), sectionResult(b.value(key)),
            digest(a.value(key)), digest(b.value(key)));
    }
    auto history = before.agentActivity();
    if (history != after.agentActivity()) {
        QSet<QString> batchIds;
        for (const auto &v : history)
            batchIds.insert(v.toObject()["id"].toString());
        for (const auto &v : after.agentActivity())
            if (!batchIds.contains(v.toObject()["id"].toString()))
                history.append(v);
    }
    if (edits.isEmpty()) {
        after.setAgentActivity(history);
        return;
    }
    QJsonArray details;
    if (!sections.isEmpty()) {
        // Reuse the editor's domain-specific descriptions for animation, scenes,
        // artwork and other compound formats. The ordinary report compares models.
        for (const auto &entry : after.changeReport(&before, false).entries) {
            if (entry.location.editor == "classes" || entry.location.editor == "items" ||
                entry.location.editor == "consumables" || entry.area == "Code patches")
                continue;
            details.append(QJsonObject{{"area", entry.area},
                                       {"subject", entry.subject},
                                       {"details", QJsonArray::fromStringList(entry.details)}});
        }
        if (sections.size() == 1 && !details.isEmpty()) {
            auto entry = details.first().toObject();
            const auto lines = entry["details"].toArray();
            auto row = edits.last().toObject();
            row["description"] =
                entry["subject"].toString() +
                (lines.isEmpty() ? QString() : " · " + lines.first().toString().left(180));
            edits.replace(edits.size() - 1, row);
        }
    }
    history.append(
        QJsonObject{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                    {"time", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                    {"source", source},
                    {"edits", edits},
                    {"actions", QJsonArray::fromStringList(actions)},
                    {"details", details}});
    after.setAgentActivity(history);
}
QJsonArray agentEditRows(const Project &p) {
    QJsonArray out;
    const auto batches = p.agentActivity();
    if (batches.isEmpty())
        return out;
    const auto sections = state(p);
    const auto patchMap = patches(p);
    QSet<QByteArray> newer;
    for (auto it = batches.size(); it > 0; --it) {
        const auto batch = batches[it - 1].toObject();
        for (const auto &v : batch["edits"].toArray()) {
            auto row = v.toObject();
            const auto scope = row["scope"].toObject();
            const auto key = QJsonDocument(scope).toJson(QJsonDocument::Compact);
            const auto value =
                newer.contains(key) ? QString() : currentDigest(p, scope, sections, patchMap);
            row["status"] = newer.contains(key)                 ? "Earlier edit"
                            : value == row["after"].toString()  ? "Matches project"
                            : value == row["before"].toString() ? "Reverted"
                                                                : "Changed since";
            newer.insert(key);
            row["batch"] = batch["id"];
            row["time"] = batch["time"];
            row["source"] = batch["source"];
            out.append(row);
        }
    }
    return out;
}
} // namespace lordly
