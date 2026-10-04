#include "engine/legacy_scene_arena.h"
#include "engine/scene_document_compiler.h"
#include "engine/scene_events.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
namespace lordly {
std::optional<LegacySceneArena> LegacySceneArena::read(const Rom &rom) {
    rom.requireSupported();
    const auto &bytes = rom.bytes();
    // Foreign tail bytes are not ours to interpret or reclaim.
    if (bytes.size() < end || bytes.mid(descriptor, 4) != "OBCS")
        return {};
    auto fail = [] { throw Error("Browser scene arena has damaged ownership or metadata"); };
    const auto version = u32(bytes, descriptor + 4), count = u32(bytes, descriptor + 8),
               dataEnd = u32(bytes, descriptor + 12);
    if (rom.revision() || (version != 1 && version != 2) || count > 144 || dataEnd < start ||
        dataEnd > descriptor || dataEnd % 2 || u32(bytes, descriptor + 16) != 0x594280 ||
        u32(bytes, descriptor + 20) != 0x1f3ca88 ||
        bytes.mid(descriptor + 24, 40) != QByteArray(40, char(-1)) ||
        bytes.mid(descriptor + 64 + count * 28, 4096 - 64 - count * 28) !=
            QByteArray(4096 - 64 - count * 28, char(-1)) ||
        bytes.mid(dataEnd, descriptor - dataEnd) != QByteArray(descriptor - dataEnd, char(-1)))
        fail();
    struct Record {
        quint32 kind, owner, key, at, size, decoded, hashPrefix;
    };
    QVector<Record> records;
    QMap<int, Record> authored;
    QSet<int> eventRows;
    LegacySceneArena result;
    bool hasManifest = false;
    for (quint32 index = 0; index < count; ++index) {
        const auto at = descriptor + 64 + index * 28;
        Record record{u32(bytes, at) >> 28, u32(bytes, at) & 0xfffffff, u32(bytes, at + 4),
                      u32(bytes, at + 8),   u32(bytes, at + 12),        u32(bytes, at + 20),
                      u32(bytes, at + 24)};
        const auto refs = u32(bytes, at + 16);
        if (record.kind > 3 || (version == 1 && record.kind) ||
            quint64(record.key) + 0x594280 != record.at || record.at < start || record.at % 2 ||
            !record.size || quint64(record.at) + 4 + record.size > dataEnd ||
            u32(bytes, record.at) != record.size)
            fail();
        for (const auto &prior : records)
            if ((prior.kind == record.kind && prior.owner == record.owner) ||
                (prior.at < record.at + 4 + record.size && record.at < prior.at + 4 + prior.size))
                fail();
        records << record;
        if (record.kind == 1 || record.kind == 2) {
            const auto root = record.kind == 1 ? 0x1f3ca84U : SceneEvents::directory;
            const auto rows = record.kind == 1 ? 1693U : 115U;
            if (record.owner >= rows || refs != 1 || u32(bytes, root) != rows * 4 ||
                u32(bytes, root + 4 + record.owner * 4) != record.key || record.size < 4 ||
                record.decoded != u32(bytes, record.at + 4) || !record.decoded ||
                record.decoded > 131070)
                fail();
            const auto decoded =
                decodeLzss(bytes.mid(record.at + 8, record.size - 4), record.decoded);
            if (sha256(decoded).first(8).toUInt(nullptr, 16) != record.hashPrefix)
                throw Error("Browser scene arena program differs from its recorded digest");
            // An authored/event owner must not also be claimed by another directory.
            for (const auto directory :
                 {0x9398e8U, 0x1e24c1eU, 0x1f3ca84U, 0x594280U + 0x1a3b7b2U}) {
                const auto extent = u32(bytes, directory);
                if (extent % 4 || extent > 262144 ||
                    quint64(directory) + 4 + extent > quint64(bytes.size()))
                    fail();
                for (quint32 word = directory + 4; word < directory + 4 + extent; word += 4)
                    if (u32(bytes, word) == record.key && word != root + 4 + record.owner * 4)
                        fail();
            }
            if (record.kind == 1)
                authored.insert(int(record.owner), record);
            else
                eventRows.insert(int(record.owner));
        } else if (record.kind == 3) {
            if (record.owner || refs || hasManifest || record.decoded != record.size ||
                record.hashPrefix)
                fail();
            hasManifest = true;
            QJsonParseError error;
            const auto doc = QJsonDocument::fromJson(bytes.mid(record.at + 4, record.size), &error);
            if (error.error != QJsonParseError::NoError || !doc.isObject())
                fail();
            result.manifest = doc.object();
        }
    }
    if (!hasManifest) {
        if (!authored.isEmpty() || !eventRows.isEmpty())
            fail();
        return result;
    }
    const auto manifest = result.manifest;
    if (manifest["format"] != "ob64-cutscene-arena-manifest" || manifest["version"] != 1 ||
        !manifest["authoredScenes"].isArray())
        fail();
    QSet<QString> identities;
    QSet<int> selectors;
    QMap<QString, int> authoredSelectors;
    for (const auto value : manifest["authoredScenes"].toArray()) {
        const auto row = value.toObject();
        const auto id = row["authoredId"].toString(), name = row["name"].toString();
        const auto selector = row["selectorRow"].toInt(-1);
        if (!QRegularExpression("^[0-9a-f]{32}$").match(id).hasMatch() || identities.contains(id) ||
            name.trimmed().isEmpty() || name.size() > 120 || !authored.contains(selector) ||
            selectors.contains(selector) || row["selectorRow"].toDouble() != selector ||
            Directors::reservedSelectors().contains(selector) || row["sceneClass"] != 5 ||
            !row["templateSceneId"].isString())
            fail();
        const auto record = authored[selector];
        if (record.decoded < 92 || record.decoded > 65536 || record.decoded % 4)
            fail();
        const auto decoded = decodeLzss(bytes.mid(record.at + 8, record.size - 4), record.decoded);
        if (u32(decoded, 0) != 0x80000006 || u32(decoded, 8) != 0x36 || u32(decoded, 40) != 0x35 ||
            decoded.last(20) != QByteArray::fromHex("0000007d800000028000000080000001ff000005") ||
            DirectorProgram::decode(decoded).resourceClass() != 5)
            fail();
        identities.insert(id);
        selectors.insert(selector);
        authoredSelectors[id] = selector;
        result.authored << AuthoredSceneBuild{id, name, selector, decoded};
    }
    if (selectors.size() != authored.size() ||
        (manifest.contains("eventBindings") && !manifest["eventBindings"].isArray()))
        fail();
    QSet<QString> sites;
    for (const auto value : manifest["eventBindings"].toArray()) {
        const auto binding = value.toObject(), target = binding["target"].toObject();
        const auto id = binding["launchSiteId"].toString();
        const auto &site = SceneEvents::site(id);
        int selector = -1;
        if (target["kind"] == "authored")
            selector = authoredSelectors.value(target["authoredId"].toString(), -1);
        else if (target["kind"] == "physical") {
            const auto ref =
                SceneDocumentCompiler::catalog()[target["sceneId"].toString()].toObject();
            selector = target["selectorRow"].toInt(-1);
            if (target["selectorRow"].toDouble() != selector ||
                !ref["selectors"].toArray().contains(selector))
                fail();
        }
        if (selector < 0 || selector >= 1693 || !u32(bytes, 0x1f3ca88 + selector * 4) ||
            sites.contains(id))
            fail();
        sites.insert(id);
        if (selector != site.selector)
            result.eventBindings[id] = selector;
    }
    QMap<QString, int> observed;
    for (const auto row : eventRows) {
        const auto bindings = SceneEvents::discoverRow(SceneEvents::retailRow(rom, row),
                                                       SceneEvents::readRow(rom, row), row);
        for (auto it = bindings.cbegin(); it != bindings.cend(); ++it)
            observed[it.key()] = it.value();
    }
    if (observed != result.eventBindings)
        fail();
    return result;
}
} // namespace lordly
