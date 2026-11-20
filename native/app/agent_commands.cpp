#include "app/agent_commands.h"
#include "app/agent_disassembly.h"
#include "engine/agent_activity.h"
#include "engine/archive.h"
#include "engine/consumable_visual.h"
#include "engine/project_references.h"
#include "engine/scenario_seeder.h"
#include "rebuild/compiler.h"
#include <QBuffer>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <limits>

namespace lordly {
namespace {
struct Domain {
    std::function<QJsonObject(Project &)> read;
    void (Project::*write)(const QJsonObject &);
};
template <class T>
Domain domain(void (Project::*load)(), const std::optional<T> &(Project::*get)() const,
              void (Project::*set)(const QJsonObject &)) {
    return {[=](Project &p) {
                (p.*load)();
                const auto &value = (p.*get)();
                if (!value)
                    throw Error("This domain is unavailable on this ROM");
                return value->json();
            },
            set};
}
const QMap<QString, Domain> domains = {
    {"attacks", domain(&Project::loadAttacks, &Project::attacks, &Project::setAttacks)},
    {"campaignMap",
     domain(&Project::loadCampaignMap, &Project::campaignMap, &Project::setCampaignMap)},
    {"animationSequences", domain(&Project::loadAnimationSequences, &Project::animationSequences,
                                  &Project::setAnimationSequences)},
    {"animationRoutes", domain(&Project::loadAnimationRoutes, &Project::animationRoutes,
                               &Project::setAnimationRoutes)},
    {"animationOverrides", domain(&Project::loadAnimationOverrides, &Project::animationOverrides,
                                  &Project::setAnimationOverrides)},
    {"combatArtwork",
     domain(&Project::loadCombatArtwork, &Project::combatArtwork, &Project::setCombatArtwork)},
    {"armyArtwork",
     domain(&Project::loadArmyArtwork, &Project::armyArtwork, &Project::setArmyArtwork)},
    {"avatarArtwork",
     domain(&Project::loadAvatarArtwork, &Project::avatarArtwork, &Project::setAvatarArtwork)},
    {"categoryIcons",
     domain(&Project::loadCategoryIcons, &Project::categoryIcons, &Project::setCategoryIcons)},
    {"iconArtwork",
     domain(&Project::loadIconArtwork, &Project::iconArtwork, &Project::setIconArtwork)},
    {"promotion", domain(&Project::loadPromotion, &Project::promotion, &Project::setPromotion)},
    {"shops", domain(&Project::loadShops, &Project::shops, &Project::setShops)},
    {"encounters", domain(&Project::loadEncounters, &Project::encounters, &Project::setEncounters)},
    {"training", domain(&Project::loadTraining, &Project::training, &Project::setTraining)},
    {"scenarios", domain(&Project::loadScenarios, &Project::scenarios, &Project::setScenarios)},
    {"world", domain(&Project::loadWorld, &Project::world, &Project::setWorld)},
    {"mapEdits", domain(&Project::loadMapEdits, &Project::mapEdits, &Project::setMapEdits)},
    {"scenePrograms",
     domain(&Project::loadScenePrograms, &Project::scenePrograms, &Project::setScenePrograms)},
    {"sceneEvents",
     domain(&Project::loadSceneEvents, &Project::sceneEvents, &Project::setSceneEvents)},
    {"dialogues", domain(&Project::loadDialogues, &Project::dialogues, &Project::setDialogues)},
    {"consumableEffects", domain(&Project::loadConsumableEffects, &Project::consumableEffects,
                                 &Project::setConsumableEffects)},
    {"tools", domain(&Project::loadTools, &Project::tools, &Project::setTools)},
    {"nameTables", domain(&Project::loadNameTables, &Project::nameTables, &Project::setNameTables)},
    {"movementTypes",
     {[](Project &p) { return p.movementTypes().json(); }, &Project::setMovementTypes}},
    {"weaponTypes", {[](Project &p) { return p.weaponTypes().names(); }, &Project::setWeaponTypes}},
    {"personalNames",
     {[](Project &p) { return p.personalNames().json(); }, &Project::setPersonalNames}}};
const QMap<QString, QString> operations = {
    {"agent.edits",
     "project CLI activity, newest first, with status against current project values"},
    {"lookup", "query words; search shipped reference passages and the per-function intake table. "
               "Returns the file, line and matching text."},
    {"symbol", "name; exact matched function, its source, original owned ranges and symbol "
               "bindings, and other accepted sources that reference it."},
    {"missions",
     "list testable mission choices in the input ROM. Export first to test project edits."},
    {"mission.seed", "choice from missions, optional army (base64 battery save) and slot "
                     "(zero-based). Returns a prepared battery save as binary."},
    {"domains", "list editable project domains"},
    {"uses", "kind, id and optional key; find references through the editor and return navigable "
             "locations"},
    {"issues", "review current export issues and their editor locations"},
    {"domain", "name; read the desktop domain model"},
    {"domain.set", "name and value object; apply the desktop domain validator"},
    {"create", "table (classes or items), name, optional template ID"},
    {"text", "bank and id; omit bank to list banks"},
    {"text.set", "bank, id and text"},
    {"catalog", "optional name of a record choice table"},
    {"scene",
     "selector; inspect the current project script, including edited and newly authored scenes"},
    {"icon", "table (items or consumables) and id; render current icon as PNG"},
    {"use.animation", "id; return all four current menu frames as PNG"},
    {"records", "table (classes, items, consumables), optional ids array, query and fields array; "
                "returns current values"},
    {"fields", "table; field keys, labels, allowed ranges and choice names"},
    {"set",
     "table, ids array, values object keyed by field; change every selected record together"},
    {"manifest", "current native project document, including authored assets"},
    {"apply",
     "manifest object containing a complete native project; uses the desktop project loader"},
    {"changes", "human-readable project changes"},
    {"files", "optional query substring in packaged game sources, metadata and reference docs; "
              "optional content substring"},
    {"read", "path from files or a region assembly path; optional one-based line and count for "
             "text excerpts"},
    {"find", "path and literal query; return matching line numbers and text from packaged source, "
             "documents or reconstructed assembly"},
    {"owners", "optional query for a function name, path or linked symbol; returns code and data "
               "placements, headers and bindings"},
    {"regions", "optional query, kind or containing offset. Lists physical US Rev 0 ranges across "
                "the cartridge."},
    {"region", "name or assembly path from regions. Returns placements and assembled words with "
               "decoded instructions, or raw region bytes."},
    {"assembly", "offset in normalized ROM, size in bytes, RAM address; returns instruction words "
                 "and mnemonics"},
    {"bytes",
     "offset in normalized ROM, size in bytes; returns original input data as base64 and digest"},
    {"assets", "optional query; lists named game archives from the input ROM"},
    {"asset", "offset of an archive header from assets; returns its decoded contents"},
    {"toolchain", "authenticated tools directory, executables and build manifest"},
    {"patches", "list project-owned C and assembly patches"},
    {"patch.put", "patch object with id, language and source. Use owner for a matched single-entry "
                  "function, offset/address/size/expectedSha256 for an explicit range, or "
                  "module/expectedSourceSha256 from --build-plan for a relocated C module."},
    {"patch.enable", "id and enabled boolean"},
    {"patch.remove", "id"}};
QString required(const QJsonObject &o, const QString &key) {
    if (!o[key].isString() || o[key].toString().isEmpty())
        throw Error("Expected text for " + key);
    return o[key].toString();
}
qint64 integer(const QJsonValue &v, const QString &key) {
    if (!v.isDouble() || v.toDouble() != double(v.toInteger(-1)) || v.toDouble() < -2147483648. ||
        v.toDouble() > 4294967295.)
        throw Error("Invalid integer for " + key);
    return v.toInteger();
}
QByteArray fileBytes(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        throw Error("Cannot read " + path);
    return f.readAll();
}
QJsonObject binary(const QByteArray &bytes) {
    return {{"size", bytes.size()},
            {"sha256", QString::fromLatin1(sha256(bytes))},
            {"base64", QString::fromLatin1(bytes.toBase64())}};
}
Table tableOf(const QJsonObject &o) {
    for (auto table : {Table::Classes, Table::Items, Table::Consumables})
        if (o["table"] == tableKey(table))
            return table;
    throw Error("Expected table classes, items or consumables");
}
int column(Table table, const QString &key) {
    const auto &fs = fields(table);
    for (int i = 0; i < fs.size(); ++i)
        if (fs[i].key == key)
            return i;
    throw Error("Unknown " + tableKey(table) + " field: " + key);
}
QVector<int> rows(const Project &p, Table t, const QJsonObject &o, bool explicitIds) {
    QVector<int> result;
    if (!o.contains("ids") && !explicitIds) {
        for (int i = 0; i < p.records(t).size(); ++i)
            result.append(i);
    } else {
        if (!o["ids"].isArray() || o["ids"].toArray().isEmpty())
            throw Error("Expected nonempty ids array");
        for (const auto &v : o["ids"].toArray()) {
            auto id = integer(v, "record ID");
            if (id < 0 || id >= p.records(t).size())
                throw Error("Unknown record ID");
            if (!result.contains(int(id)))
                result.append(int(id));
        }
    }
    return result;
}
QString packageFile(const QDir &root, const QString &name) {
    if (!name.startsWith("game/") && !name.startsWith("reference/"))
        throw Error("Select a path returned by files");
    auto absolute = QFileInfo(root.filePath(name)).canonicalFilePath();
    if (absolute.isEmpty() || !absolute.startsWith(root.canonicalPath() + '/', Qt::CaseInsensitive))
        throw Error("Missing package file: " + name);
    return absolute;
}
QJsonArray catalog(const QDir &root) {
    auto doc =
        QJsonDocument::fromJson(fileBytes(packageFile(root, "game/accepted/rom-owners.json")));
    if (!doc["sources"].isArray())
        throw Error("Invalid matching C catalog");
    return doc["sources"].toArray();
}
QJsonValue regionQuery(const Project &p, const QDir &root, const QJsonObject &o) {
    if (p.runtimeRom().revision() != 0)
        throw Error("The physical source map covers US Rev 0");
    const auto index =
        QJsonDocument::fromJson(fileBytes(packageFile(root, "game/accepted/rom-index.json")))
            .object();
    QJsonArray result;
    const auto query = o["query"].toString(), kind = o["kind"].toString();
    const auto addressFilter = o.contains("offset") ? integer(o["offset"], "offset") : -1;
    const bool single = o["op"] == "region";
    const auto name = single ? required(o, "name") : QString();
    for (const auto &v : index["regions"].toArray()) {
        auto r = v.toObject();
        const auto start = r["offset"].toInteger(), size = r["size"].toInteger();
        if (single && name != r["name"] && name != r["assembly"])
            continue;
        if (!kind.isEmpty() && kind != r["kind"])
            continue;
        if (addressFilter >= 0 && (addressFilter < start || addressFilter >= start + size))
            continue;
        if (!r["name"].toString().contains(query, Qt::CaseInsensitive) &&
            !r["assembly"].toString().contains(query, Qt::CaseInsensitive) &&
            !r["kind"].toString().contains(query, Qt::CaseInsensitive))
            continue;
        if (!single) {
            result.append(r);
            continue;
        }
        QSet<quint32> addresses;
        QJsonArray mappings;
        for (const auto &map : index["placements"].toArray()) {
            auto m = map.toObject();
            if (start >= m["start"].toInteger() && start + size <= m["end"].toInteger()) {
                const auto address = m["address"].toInteger() + start - m["start"].toInteger();
                addresses.insert(quint32(address));
                mappings.append(QJsonObject{{"name", m["name"]}, {"address", address}});
            }
        }
        r["placements"] = mappings;
        const auto &bytes = p.runtimeRom().bytes();
        if (start + size > bytes.size())
            throw Error("Source region outside this ROM");
        r["sha256"] = QString::fromLatin1(sha256(bytes.mid(start, size)));
        if (r.contains("assembly")) {
            QString source;
            const bool decode = r["kind"] == "code" && !r.contains("processor");
            const bool knownAddress = addresses.size() == 1;
            const quint32 base = knownAddress ? *addresses.cbegin() : 0;
            if (size % 4)
                throw Error("Unaligned assembly region");
            for (qint64 i = 0; i < size; i += 4) {
                const auto word = u32(bytes, start + i);
                source += QString(".word 0x%1").arg(word, 8, 16, QChar('0'));
                if (decode)
                    source += " # " + disassembleGameWord(base + quint32(i), word, knownAddress);
                source += '\n';
            }
            r["source"] = source;
        } else
            r["bytes"] = binary(bytes.mid(start, size));
        return r;
    }
    if (single)
        throw Error("Unknown physical ROM region: " + name);
    return result;
}
QJsonObject preparePatch(const QDir &root, QJsonObject patch) {
    if (!patch.contains("owner"))
        return patch;
    const auto name = required(patch, "owner");
    for (const auto &v : catalog(root)) {
        auto owner = v.toObject();
        const auto targets = owner["targets"].toArray();
        if (!targets.contains(name))
            continue;
        const auto parts = owner["parts"].toArray();
        if (targets.size() != 1 || parts.size() != 1 || !parts[0].toObject()["code"].toInt())
            throw Error("This source has shared code or data. Select the exact function range from "
                        "its disassembly.");
        const auto part = parts[0].toObject();
        patch["offset"] = part["start"];
        patch["size"] = part["size"];
        patch["address"] = part["address"];
        patch["expectedSha256"] = part["sha256"];
        auto bindings = owner["bindings"].toObject();
        const auto extra = patch["bindings"].toObject();
        for (const auto &key : extra.keys())
            bindings[key] = extra[key];
        patch["bindings"] = bindings;
        if (patch["language"] == "c") {
            if (!patch.contains("entry"))
                patch["entry"] = name;
            auto headers = patch["headers"].toObject();
            for (const auto &header : owner["headers"].toArray()) {
                const auto h = header.toObject();
                const auto data = fileBytes(packageFile(root, h["file"].toString()));
                if (sha256(data) != h["sha256"].toString().toLower().toLatin1())
                    throw Error("Package header digest mismatch");
                const auto path = h["path"].toString();
                const auto key = path.startsWith("include/") ? path.mid(8) : path;
                if (!headers.contains(key))
                    headers[key] = QString::fromUtf8(data);
            }
            patch["headers"] = headers;
        }
        return patch;
    }
    throw Error("No matched function named " + name);
}
QJsonValue execute(Project &p, const QJsonObject &o, const QDir &root, const QString &tools,
                   bool &edited) {
    const auto op = required(o, "op"), query = o["query"].toString();
    if (op == "agent.edits")
        return agentEditRows(p);
    QJsonArray result;
    if (op == "lookup") {
        const auto terms = required(o, "query").simplified().split(' ', Qt::SkipEmptyParts);
        if (terms.isEmpty())
            throw Error("Supply a search term");
        const QDir references(root.filePath("reference"));
        if (!references.exists())
            throw Error("Missing portable reference library");
        for (const auto &file : references.entryList({"*.md", "*.tsv"}, QDir::Files, QDir::Name)) {
            const auto path = "reference/" + file;
            auto lines = QString::fromUtf8(fileBytes(packageFile(root, path))).split('\n');
            for (int i = 0; i < lines.size(); ++i) {
                const auto first = i;
                QString passage = lines.at(i).trimmed();
                if (passage.isEmpty())
                    continue;
                if (file.endsWith(".md") && !passage.startsWith('|') && !passage.startsWith('#'))
                    while (i + 1 < lines.size() && !lines.at(i + 1).trimmed().isEmpty() &&
                           !lines.at(i + 1).startsWith('|') && !lines.at(i + 1).startsWith('#'))
                        passage += " " + lines.at(++i).trimmed();
                if (!std::all_of(terms.cbegin(), terms.cend(), [&](const auto &term) {
                        return passage.contains(term, Qt::CaseInsensitive);
                    }))
                    continue;
                QJsonObject hit{{"path", path}, {"line", first + 1}, {"text", passage}};
                if (file.endsWith(".tsv") && first)
                    hit["symbol"] = passage.section('\t', 0, 0);
                result.append(hit);
            }
        }
        return result;
    }
    if (op == "symbol") {
        const auto name = required(o, "name");
        QJsonObject owner;
        QJsonArray references;
        for (const auto &v : catalog(root)) {
            const auto row = v.toObject();
            if (row["targets"].toArray().contains(name))
                owner = row;
            const auto bindings = row["bindings"].toObject();
            if (bindings.contains(name))
                references.append(QJsonObject{{"source", row["source"]},
                                              {"targets", row["targets"]},
                                              {"linkedAddress", bindings[name]}});
        }
        if (owner.isEmpty() && references.isEmpty())
            throw Error("Unknown game symbol: " + name);
        return QJsonObject{{"name", name}, {"owner", owner}, {"referencedBy", references}};
    }
    if (op == "missions" || op == "mission.seed") {
        if (p.runtimeRom().revision())
            throw Error("Mission seeding requires US Rev 0");
        const ScenarioSeeder seeder(p.runtimeRom());
        if (op == "missions") {
            for (const auto &choice : seeder.choices())
                result.append(QJsonObject{{"choice", result.size()},
                                          {"mission", choice.mission},
                                          {"location", choice.location},
                                          {"event", choice.route.event},
                                          {"node", choice.route.node}});
            return result;
        }
        const auto choice = integer(o["choice"], "choice");
        if (choice < 0 || choice >= seeder.choices().size())
            throw Error("Unknown mission choice");
        std::optional<SaveDocument> donor;
        if (o.contains("army")) {
            const auto bytes = QByteArray::fromBase64(required(o, "army").toLatin1(),
                                                      QByteArray::AbortOnBase64DecodingErrors);
            donor = SaveDocument::open(seeder.rom(), bytes, p.records(Table::Classes).size());
            if (!donor->battery())
                throw Error("Supply a battery save for a donor army");
            if (o.contains("slot"))
                donor->selectSlot(int(integer(o["slot"], "slot")));
        } else if (o.contains("slot"))
            throw Error("slot requires a donor army");
        return binary(seeder.seed(int(choice), donor ? &*donor : nullptr));
    }
    if (op == "region" || op == "regions")
        return regionQuery(p, root, o);
    if (op == "domains")
        return QJsonArray::fromStringList(domains.keys());
    if (op == "uses" || op == "issues") {
        auto location = [](const EditLocation &target) {
            return QJsonObject{{"editor", target.editor},
                               {"id", target.id},
                               {"child", target.child},
                               {"key", target.key}};
        };
        if (op == "issues") {
            for (const auto &issue : exportIssues(p))
                result.append(QJsonObject{{"message", issue.message},
                                          {"blocking", issue.blocking},
                                          {"location", location(issue.location)}});
        } else {
            const auto id = integer(o["id"], "id");
            if (id < 0 || id > std::numeric_limits<int>::max())
                throw Error("Invalid reference ID");
            for (const auto &ref :
                 findProjectUses(p, required(o, "kind"), int(id), o["key"].toString()))
                result.append(QJsonObject{{"area", ref.area},
                                          {"subject", ref.subject},
                                          {"details", QJsonArray::fromStringList(ref.details)},
                                          {"location", location(ref.location)}});
        }
        return result;
    }
    if (op == "domain" || op == "domain.set") {
        const auto name = required(o, "name");
        const auto it = domains.constFind(name);
        if (it == domains.cend())
            throw Error("Unknown domain: " + name);
        if (op == "domain")
            return it->read(p);
        if (!o["value"].isObject())
            throw Error("Expected a domain value object");
        (p.*(it->write))(o["value"].toObject());
        edited = true;
        return name;
    }
    if (op == "catalog")
        return o.contains("name") ? p.catalog().value(required(o, "name"))
                                  : QJsonValue(p.catalog());
    if (op == "scene") {
        p.loadScenePrograms();
        const auto directors = p.scenePrograms()->runtimeDirectors();
        const auto selector = integer(o["selector"], "selector");
        if (selector < 0 || selector >= directors.selectors().size())
            throw Error("Unknown scene selector");
        const auto entry = directors.entries().constFind(directors.selectors().at(selector));
        if (entry == directors.entries().cend())
            throw Error("No scene for selector");
        for (const auto &command : entry->program.commands) {
            QJsonArray operands;
            for (auto value : command.operands)
                operands.append(qint64(value));
            result.append(QJsonObject{{"word", command.word},
                                      {"opcode", qint64(command.opcode)},
                                      {"name", Directors::grammar().value(command.opcode).name},
                                      {"operands", operands}});
        }
        return QJsonObject{{"name", entry->name}, {"selector", selector}, {"commands", result}};
    }
    if (op == "text" || op == "text.set") {
        p.loadTexts();
        if (op == "text" && !o.contains("bank"))
            return QJsonArray::fromStringList(p.textBanks().keys());
        const auto bank = required(o, "bank");
        const auto id = integer(o["id"], "id");
        if (!p.textBanks().contains(bank) || id < 0 || id >= p.textBanks()[bank].entries.size())
            throw Error("Unknown text entry");
        if (op == "text.set") {
            if (!o["text"].isString())
                throw Error("Expected text");
            p.setText(bank, int(id), o["text"].toString());
            edited = true;
        }
        return p.textBanks()[bank].text(int(id));
    }
    if (op == "create") {
        const auto t = tableOf(o);
        const auto name = required(o, "name");
        if (t == Table::Consumables)
            throw Error("Choose classes or items");
        int id;
        if (o.contains("template")) {
            const auto from = integer(o["template"], "template");
            if (from < 0 || from >= p.records(t).size())
                throw Error("Unknown template");
            id = t == Table::Classes ? p.appendClass(int(from), name)
                                     : p.appendItem(int(from), name);
        } else
            id = t == Table::Classes ? p.createClass(name) : p.createItem(name);
        edited = true;
        return id;
    }
    if (op == "icon" || op == "use.animation") {
        const auto id = integer(o["id"], "id");
        const auto t = op == "icon" ? tableOf(o) : Table::Consumables;
        if (t == Table::Classes || id <= 0 || id >= p.records(t).size())
            throw Error("Unknown item");
        auto png = [](const QImage &image) {
            QByteArray bytes;
            QBuffer out(&bytes);
            out.open(QIODevice::WriteOnly);
            if (!image.save(&out, "PNG"))
                throw Error("Cannot encode image");
            auto obj = binary(bytes);
            obj["mime"] = "image/png";
            return obj;
        };
        if (op == "icon") {
            p.loadIconArtwork();
            return png(p.iconArtwork()->image(quint16(id) | (t == Table::Consumables ? 0x200 : 0)));
        }
        p.loadConsumableEffects();
        auto visuals = p.consumableEffects()->visuals();
        auto key = QString::number(id);
        const auto visual = visuals.contains(key)
                                ? ConsumableVisual::fromJson(visuals[key].toObject())
                                : ConsumableVisual::retail(p.runtimeRom(), int(id));
        for (int i = 0; i < 4; ++i)
            result.append(png(visual.frame(i)));
        return result;
    }
    if (op == "fields" || op == "records" || op == "set") {
        const auto t = tableOf(o);
        if (op == "fields") {
            for (const auto &f : fields(t))
                result.append(QJsonObject{{"key", f.key},
                                          {"label", f.label},
                                          {"group", f.group},
                                          {"minimum", f.minimum()},
                                          {"maximum", f.maximum()},
                                          {"choices", p.catalog()[f.choices]}});
            return result;
        }
        const auto changes = o["values"].toObject();
        if (op == "set" && changes.isEmpty())
            throw Error("set needs values");
        QStringList keys;
        if (op == "set")
            keys = changes.keys();
        else if (o.contains("fields")) {
            if (!o["fields"].isArray())
                throw Error("fields must be an array");
            for (const auto &k : o["fields"].toArray())
                keys << k.toString();
        } else
            for (const auto &f : fields(t))
                keys << f.key;
        for (const auto id : rows(p, t, o, op == "set")) {
            const auto &r = p.record(t, id);
            if (!r.name.contains(query, Qt::CaseInsensitive))
                continue;
            QJsonObject values;
            for (const auto &key : keys) {
                int col = column(t, key);
                if (op == "set") {
                    const auto n = integer(changes[key], key);
                    if (n > std::numeric_limits<int>::max())
                        throw Error("Out of range: " + key);
                    p.setValue(t, id, col, int(n));
                    edited = true;
                }
                values[key] = p.value(t, id, col);
            }
            result.append(QJsonObject{{"id", id}, {"name", r.name}, {"values", values}});
        }
        return result;
    }
    if (op == "manifest")
        return p.manifest();
    if (op == "apply") {
        p.applyManifest(o["manifest"].toObject());
        edited = true;
        return true;
    }
    if (op == "changes")
        return p.changeReport().text();
    if (op == "files" || op == "read" || op == "find") {
        if (op != "files") {
            const auto path = required(o, "path");
            QString content;
            if (path.startsWith("asm/original/rev0/")) {
                content = regionQuery(p, root, {{"op", "region"}, {"name", path}})
                              .toObject()["source"]
                              .toString();
            } else {
                const auto data = fileBytes(packageFile(root, path));
                content = QString::fromUtf8(data);
                if (data.contains('\0') || content.toUtf8() != data) {
                    if (op == "find" || o.contains("line") || o.contains("count"))
                        throw Error("Text search and line ranges require a text file");
                    return binary(data);
                }
            }
            if (op == "find") {
                const auto term = required(o, "query");
                const auto lines = content.split('\n');
                for (int i = 0; i < lines.size(); ++i)
                    if (lines[i].contains(term, Qt::CaseInsensitive))
                        result.append(QJsonObject{{"line", i + 1}, {"text", lines[i]}});
                return result;
            }
            if (!o.contains("line") && !o.contains("count"))
                return content;
            const auto line = o.contains("line") ? integer(o["line"], "line") : 1;
            const auto count = o.contains("count") ? integer(o["count"], "count") : 40;
            const auto lines = content.split('\n');
            if (line < 1 || line > lines.size() || count < 1 || count > 4096)
                throw Error("Invalid text line range");
            return QJsonObject{{"path", path},
                               {"line", line},
                               {"totalLines", lines.size()},
                               {"text", lines.mid(line - 1, count).join('\n')}};
        }
        for (const auto folder : {QString("game"), QString("reference")}) {
            QDirIterator it(root.filePath(folder), QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const auto name = root.relativeFilePath(it.next());
                if (!name.contains(query, Qt::CaseInsensitive))
                    continue;
                if (o.contains("content") &&
                    !QString::fromUtf8(fileBytes(packageFile(root, name)))
                         .contains(required(o, "content"), Qt::CaseInsensitive))
                    continue;
                result.append(name);
            }
        }
        return result;
    }
    if (op == "owners") {
        for (const auto &v : catalog(root)) {
            const auto owner = v.toObject();
            if (owner["source"].toString().contains(query, Qt::CaseInsensitive) ||
                QString::fromUtf8(
                    QJsonDocument(owner["targets"].toArray()).toJson(QJsonDocument::Compact))
                    .contains(query, Qt::CaseInsensitive) ||
                owner["bindings"].toObject().keys().join(' ').contains(query, Qt::CaseInsensitive))
                result.append(v);
        }
        return result;
    }
    if (op == "bytes" || op == "assembly") {
        const auto start = integer(o["offset"], "offset"), size = integer(o["size"], "size");
        const auto &data = p.runtimeRom().bytes();
        if (start < 0 || size <= 0 || start + size > data.size())
            throw Error("Requested range outside input ROM");
        if (op == "bytes")
            return binary(data.mid(start, size));
        const auto address = integer(o["address"], "RAM address");
        if (address < 0 || address + size > 0x100000000LL || (start | size | address) % 4)
            throw Error("Invalid instruction range");
        for (qint64 i = 0; i < size; i += 4) {
            auto word = u32(data, start + i);
            result.append(
                QJsonObject{{"offset", start + i},
                            {"address", address + i},
                            {"word", qint64(word)},
                            {"instruction", disassembleGameWord(quint32(address + i), word)}});
        }
        return result;
    }
    if (op == "asset") {
        auto member =
            LhaMember::read(p.runtimeRom().bytes(), integer(o["offset"], "archive offset"));
        auto data = binary(member.decode(p.runtimeRom()));
        data["name"] = member.name;
        return data;
    }
    if (op == "assets") {
        qsizetype at = 0;
        const auto &data = p.runtimeRom().bytes();
        while ((at = data.indexOf("-lh", at)) >= 0) {
            const auto header = at - 2;
            at += 3;
            try {
                const auto member = LhaMember::read(data, header);
                if (member.name.contains(query, Qt::CaseInsensitive))
                    result.append(QJsonObject{{"name", member.name},
                                              {"offset", header},
                                              {"storedSize", member.storedSize},
                                              {"decodedSize", member.decodedSize}});
            } catch (const Error &) { /* A signature inside compressed data is not a header. */
            }
        }
        return result;
    }
    if (op == "toolchain") {
        Compiler(tools).authenticate();
        auto out = QJsonDocument::fromJson(fileBytes(":/rebuild/toolchain.json")).object();
        out["directory"] = QDir(tools).absolutePath();
        return out;
    }
    if (op == "patches")
        return p.codePatches();
    if (op == "patch.put" || op == "patch.remove" || op == "patch.enable") {
        auto patches = p.codePatches();
        auto patch = o["patch"].toObject();
        const auto id = op == "patch.put" ? required(patch, "id") : required(o, "id");
        int index = 0;
        while (index < patches.size() && patches[index].toObject()["id"] != id)
            ++index;
        if (op == "patch.put") {
            if (patch.contains("owner") && p.runtimeRom().revision() != 0)
                throw Error("Named matching C owners require a US Rev 0 input");
            patch = preparePatch(root, patch);
            if (index == patches.size())
                patches.append(patch);
            else
                patches[index] = patch;
        } else {
            if (index == patches.size())
                throw Error("No code patch named " + id);
            if (op == "patch.remove")
                patches.removeAt(index);
            else {
                if (!o["enabled"].isBool())
                    throw Error("patch.enable requires a boolean");
                patch = patches[index].toObject();
                patch["enabled"] = o["enabled"];
                patches[index] = patch;
            }
        }
        p.setCodePatches(patches);
        edited = true;
        return id;
    }
    throw Error("Unknown editor operation: " + op);
}
} // namespace
QJsonObject agentCapabilities() {
    QJsonObject result;
    for (auto it = operations.begin(); it != operations.end(); ++it)
        result[it.key()] = it.value();
    return result;
}
QJsonArray runAgentCommands(Project &project, const QJsonArray &commands, const QString &library,
                            const QString &tools, bool &edited) {
    QJsonArray result;
    std::optional<Project> before;
    QStringList actions;
    auto flush = [&] {
        if (!before)
            return;
        recordAgentEdits(*before, project, "CLI commands", actions);
        before.reset();
        actions.clear();
    };
    for (const auto &v : commands) {
        try {
            const auto command = v.toObject();
            const auto op = command["op"].toString();
            if (op == "agent.edits" || op == "changes" || op == "manifest")
                flush();
            if (op == "set" || op == "create" || op == "apply" || op.endsWith(".set") ||
                op.startsWith("patch.")) {
                if (!before)
                    before.emplace(project);
                const auto label =
                    op + " " + command["table"].toString() + command["name"].toString();
                if (!actions.contains(label.trimmed()))
                    actions.append(label.trimmed());
            }
            auto value = execute(project, command, QDir(library), tools, edited);
            if (command.contains("page")) {
                if (!value.isArray() || !command["page"].isObject())
                    throw Error("Pagination needs a list result and page object");
                const auto page = command["page"].toObject();
                const auto start = integer(page["start"], "page.start"),
                           limit = integer(page["limit"], "page.limit");
                if (start < 0 || limit <= 0)
                    throw Error("Invalid page bounds");
                const auto all = value.toArray();
                QJsonArray items;
                for (auto i = start; i < all.size() && i - start < limit; ++i)
                    items.append(all.at(i));
                value = QJsonObject{{"start", start}, {"total", all.size()}, {"items", items}};
            }
            result.append(value);
        } catch (const std::exception &e) {
            throw Error(QString("Command %1 (%2): %3")
                            .arg(result.size() + 1)
                            .arg(v.toObject()["op"].toString(), QString::fromUtf8(e.what())));
        }
    }
    flush();
    return result;
}
} // namespace lordly
