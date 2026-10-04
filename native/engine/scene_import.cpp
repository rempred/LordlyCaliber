#include "engine/scene_import.h"
#include <QJsonArray>
#include <QRegularExpression>
#include <cmath>
namespace lordly {
namespace {
int integer(const QJsonValue &value, int low, int high) {
    const auto number = value.toDouble(-1);
    if (!value.isDouble() || !std::isfinite(number) || std::floor(number) != number ||
        number < low || number > high)
        throw Error("Invalid imported scene selector or class");
    return int(number);
}
QByteArray unhex(const QJsonValue &value) {
    const auto hex = value.toString().toLatin1(), bytes = QByteArray::fromHex(hex);
    if (!value.isString() || hex.isEmpty() || hex.size() > 32 * 1024 * 1024 ||
        bytes.toHex() != hex.toLower())
        throw Error("Invalid imported scene stream");
    return bytes;
}
QJsonObject reference(const QString &id) {
    const auto found = SceneDocumentCompiler::catalog()[id].toObject();
    if (found.isEmpty())
        throw Error("Unknown imported retail scene identity: " + id);
    return found;
}
quint32 currentKey(const ScenePrograms &scenes, const QJsonObject &ref) {
    quint32 key = 0;
    for (const auto value : ref["selectors"].toArray()) {
        const auto selector = value.toInt(-1);
        if (selector < 0 || selector >= scenes.original().selectors().size())
            throw Error("Invalid scene catalog selector");
        const auto current = scenes.original().selectors()[selector];
        if (!current || (key && key != current))
            throw Error("Imported scene aliases no longer share one ROM resource");
        key = current;
    }
    if (!key || !scenes.original().entries().contains(key))
        throw Error("Imported scene is absent from the loaded ROM");
    return key;
}
} // namespace
DirectorProgram wipeSceneProgram(const DirectorProgram &source) {
    const auto &nodes = source.commands;
    if (source.resourceClass() < 0 || nodes.size() < 2)
        throw Error("This scene has no complete terminal class");
    int exit = nodes.size() - 2;
    while (exit >= 0 && (nodes[exit].opcode == 0x80000002 || nodes[exit].opcode == 0x80000000))
        --exit;
    bool handoff = false, found = false;
    if (exit >= 0 && nodes[exit].opcode == 0x7d)
        found = true;
    if (exit >= 0 && nodes[exit].opcode == 0x39 && nodes[exit].operands == QVector<quint32>{0}) {
        found = true;
        handoff = true;
        for (int i = exit + 1; i < nodes.size() - 1; ++i)
            found &=
                nodes[i].opcode == 0x80000000 || (i == exit + 1 && nodes[i].opcode == 0x80000002);
    }
    if (!found) {
        const QVector<QVector<quint32>> suffixes{
            {0x7d, 0, 0x80000002, 0x80000000, 0x80000001},
            {0x39, 0x80000000, 2, 0x80000000, 0x80000001},
            {0x39, 0x80000002, 0x80000000, 2, 0x80000000, 0x80000001}};
        for (const auto &suffix : suffixes) {
            const auto start = nodes.size() - suffix.size();
            if (start < 0)
                continue;
            bool matches = true;
            for (int i = 0; i < suffix.size(); ++i)
                matches &= nodes[start + i].opcode == suffix[i];
            if (suffix.first() == 0x39)
                matches &= nodes[start].operands == QVector<quint32>{0};
            if (matches) {
                exit = start;
                found = true;
                handoff = suffix.first() == 0x39;
                break;
            }
        }
    }
    if (!found)
        throw Error("This scene has no supported release or presentation handoff for wiping");
    if ((exit > 0 && nodes[exit - 1].opcode == 0xbb) ||
        (exit > 1 && nodes[exit - 1].opcode == 0x80000000 && nodes[exit - 2].opcode == 0xbb))
        throw Error("This scene relaunches its parent event; its empty handoff is unresolved");
    int lead = 0;
    while (lead < exit && (nodes[lead].opcode == 0x80000006 || nodes[lead].opcode == 0x80000007))
        ++lead;
    if ((source.resourceClass() == 4 || source.resourceClass() == 5) && lead + 1 < exit &&
        ((nodes[lead].opcode == 0x36 && nodes[lead + 1].opcode == 0x35) ||
         (nodes[lead].opcode == 0x35 && nodes[lead + 1].opcode == 0x36)))
        lead += 2;
    QByteArray bytes;
    auto append = [&](const DirectorCommand &node) {
        const auto at = node.word * 4;
        bytes += source.source.mid(at, (node.operands.size() + 1) * 4);
    };
    for (int i = 0; i < lead; ++i)
        append(nodes[i]);
    for (int i = exit; i < nodes.size(); ++i) {
        append(nodes[i]);
        if (i == exit && handoff && nodes[i + 1].opcode != 0x80000002)
            bytes += QByteArray::fromHex("80000002");
    }
    return DirectorProgram::decode(bytes);
}
ImportedScenes importBrowserScenes(const Rom &rom, ScenePrograms programs, SceneEvents events,
                                   const QJsonObject &payload) {
    if (payload["format"] != "ob64-cutscene-project" ||
        (payload["schemaVersion"] != 1 && payload["schemaVersion"] != 2) ||
        payload["sourceRevision"] != "us-rev0" || !payload["scenes"].isArray())
        throw Error("Unsupported browser scene project");
    for (const auto key : {"authoredScenes", "deletedAuthoredScenes", "eventBindings"})
        if (payload.contains(key) && !payload[key].isArray())
            throw Error("Invalid imported scene list: " + QString::fromLatin1(key));
    SceneDocumentCompiler compiler;
    auto manifest = programs.json(), edited = manifest["programs"].toObject();
    QMap<quint32, QByteArray> compiled;
    QMap<QString, QJsonObject> authored;
    for (const auto entry : manifest["authored"].toArray())
        authored[entry.toObject()["id"].toString()] = entry.toObject();
    QSet<QString> importedIds;
    for (const auto value : payload["authoredScenes"].toArray()) {
        const auto row = value.toObject();
        const auto id = row["authoredId"].toString();
        if (!QRegularExpression("^[0-9a-f]{32}$").match(id).hasMatch() || importedIds.contains(id))
            throw Error("Invalid or duplicate imported scene UUID");
        importedIds.insert(id);
        const auto initial = DirectorProgram::decode(unhex(row["initialStreamHex"]));
        if (initial.resourceClass() != integer(row["sceneClass"], 0, 255))
            throw Error("Imported authored scene class differs from its initial stream");
        const auto selector =
            integer(row["selectorRow"], 0, programs.original().selectors().size() - 1);
        auto result = initial.source;
        if (!row["document"].isNull() && !row["document"].isUndefined()) {
            if (!row["document"].isObject())
                throw Error("Invalid imported authored scene document");
            const auto document = row["document"].toObject();
            if (document["identity"].toObject()["sceneId"] != "scene:authored:" + id)
                throw Error("Imported authored document belongs to another scene");
            result = compiler.compile(initial, document);
        }
        if (authored.contains(id) && (authored[id]["selector"].toInt() != selector ||
                                      unhex(authored[id]["baseline"]) != initial.source))
            throw Error("Imported authored scene differs from its source ROM");
        authored[id] = {{"id", id},
                        {"name", row["name"]},
                        {"selector", selector},
                        {"baseline", QString::fromLatin1(initial.source.toHex())},
                        {"program", QString::fromLatin1(result.toHex())}};
    }
    for (const auto value : payload["scenes"].toArray()) {
        if (!value.isObject())
            throw Error("Invalid imported scene entry");
        const auto row = value.toObject(), ref = reference(row["sceneId"].toString());
        const auto document = row["document"].toObject(),
                   identity = document["identity"].toObject();
        bool keyOk = false;
        const auto identityKey = identity["directorKey"].toString().toUInt(&keyOk, 16);
        if (!row["document"].isObject() || row["assetId"] != ref["assetId"] ||
            row["storageId"] != ref["storageId"] || identity["sceneId"] != ref["sceneId"] ||
            !keyOk || identityKey != quint32(ref["key"].toInteger()) ||
            document["native"].toObject()["sourceAssetId"] != ref["assetId"])
            throw Error("Imported scene identities disagree");
        const auto key = currentKey(programs, ref);
        auto original = programs.original().entries()[key].program;
        if (row["wiped"].toBool())
            original = wipeSceneProgram(original);
        const auto result = compiler.compile(original, document, ref["mode"].toInt(-1));
        if (compiled.contains(key) && compiled[key] != result)
            throw Error("Imported scene aliases contain conflicting edits");
        if (programs.plan().contains(key) && programs.plan()[key] != result)
            throw Error("Imported scene conflicts with saved native edits");
        compiled[key] = result;
        if (result == programs.original().entries()[key].program.source)
            edited.remove(QString::number(key));
        else
            edited[QString::number(key)] = QString::fromLatin1(result.toHex());
    }
    QSet<QString> deleted;
    for (const auto value : payload["deletedAuthoredScenes"].toArray()) {
        const auto id = value.toString();
        if (!value.isString() || !authored.contains(id) || deleted.contains(id) ||
            importedIds.contains(id))
            throw Error("Invalid deleted authored scene identity");
        deleted.insert(id);
        authored.remove(id);
    }
    QJsonArray authoredRows;
    for (const auto &row : authored)
        authoredRows << row;
    manifest["authored"] = authoredRows;
    manifest["programs"] = edited;
    programs.apply(manifest);
    if (payload.contains("eventBindings")) {
        QJsonObject bindings;
        QSet<QString> identities;
        for (const auto value : payload["eventBindings"].toArray()) {
            const auto row = value.toObject();
            const auto id = row["launchSiteId"].toString();
            const auto target = row["target"].toObject();
            if (!value.isObject() || row.size() != 2 || !row["target"].isObject() ||
                identities.contains(id))
                throw Error("Invalid imported event binding");
            SceneEvents::site(id);
            identities.insert(id);
            int selector;
            if (target["kind"] == "authored") {
                const auto uuid = target["authoredId"].toString();
                if (!programs.authoredScenes().contains(uuid))
                    throw Error("Imported event targets a missing authored scene");
                selector = programs.authoredScenes()[uuid].selector;
            } else if (target["kind"] == "physical") {
                const auto ref = reference(target["sceneId"].toString());
                selector = integer(target["selectorRow"], 0, 65535);
                if (!ref["selectors"].toArray().contains(selector))
                    throw Error("Imported launch selector does not belong to its target scene");
                currentKey(programs, ref);
            } else
                throw Error("Unknown imported event target kind");
            bindings[id] = selector;
        }
        events.apply({{"version", 1}, {"bindings", bindings}}, programs);
    }
    events.validate(programs);
    return {std::move(programs), std::move(events)};
}
} // namespace lordly
