#include "engine/scenarios.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <cmath>
namespace lordly {
namespace {
int integer(const QJsonValue &v, int lo, int hi, const QString &label) {
    const double n = v.toDouble(-1);
    if (!v.isDouble() || !std::isfinite(n) || n < lo || n > hi || n != std::floor(n))
        throw Error("Invalid imported " + label);
    return int(n);
}
int keyNumber(const QString &key, int hi = 64) {
    bool ok = false;
    const int id = key.toInt(&ok);
    if (!ok || QString::number(id) != key || id < 0 || id > hi)
        throw Error("Invalid imported scenario or template key");
    return id;
}
QJsonObject object(const QJsonValue &v, const QString &label, bool optional = false) {
    if (optional && (v.isUndefined() || v.isNull()))
        return {};
    if (!v.isObject())
        throw Error("Invalid imported " + label);
    return v.toObject();
}
QJsonArray array(const QJsonValue &v, const QString &label) {
    if (v.isUndefined() || v.isNull())
        return {};
    if (!v.isArray())
        throw Error("Invalid imported " + label);
    return v.toArray();
}
void fields(const QJsonObject &value, const QSet<QString> &allowed, const QString &label) {
    for (auto it = value.begin(); it != value.end(); ++it)
        if (!allowed.contains(it.key()))
            throw Error("Unrecognized imported " + label + " field: " + it.key());
}
QByteArray hex(const QJsonValue &v, int size = -1, bool strict = true) {
    if (!v.isString())
        throw Error("Imported scenario bytes must be hexadecimal text");
    auto text = v.toString();
    if (!strict)
        text.remove(QRegularExpression("\\s+"));
    const auto ascii = text.toLatin1(), bytes = QByteArray::fromHex(ascii);
    if (bytes.toHex() != ascii.toLower() || (size >= 0 && bytes.size() != size))
        throw Error("Invalid imported scenario byte length or hexadecimal text");
    return bytes;
}
QString recordKey(int key, int record) {
    return QString("%1:%2").arg(key).arg(record);
}
void offsets(const QJsonValue &v, const QByteArray &record, bool required) {
    if (v.isUndefined() && !required)
        return;
    const auto values = object(v, "cohort level offsets");
    const QStringList names{"A", "B", "C"};
    const std::array<int, 3> at{1, 8, 17};
    if (values.size() != 3)
        throw Error("Imported level offsets must name A, B and C");
    for (int i = 0; i < 3; ++i)
        if (integer(values[names[i]], 0, 255, "cohort offset") != u8(record, at[i]))
            throw Error("Imported cohort offsets disagree with their squad record");
}
} // namespace
void Scenarios::importBrowser(const Rom &rom, const QJsonObject &input,
                              const QJsonObject &overrides, const ClassData &classes) {
    if (input.isEmpty() && overrides.isEmpty())
        return;
    if (modified())
        throw Error("Import the browser scenario project from its source ROM before making native "
                    "scenario edits");
    auto next = *this;
    auto project = input;
    const bool present = !project.isEmpty();
    const int version = present ? integer(project["version"], 1, 4, "scenario project version") : 0;
    if (present && project["format"] != "ob64-scenario-project")
        throw Error("Unrecognized browser scenario project format");
    const bool strict = version >= 4;
    QFile file(":/game/scenarios/import-catalog.json");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Scenario import identities are unavailable");
    const auto catalog = QJsonDocument::fromJson(file.readAll()).object()["scenarios"].toObject();
    const auto resources = ScenarioResources::load(rom);
    auto metadata = [&](int key) {
        next.scenario(key);
        const auto value = catalog[QString::number(key)].toObject();
        if (value.isEmpty())
            throw Error("Missing browser scenario identity");
        return value;
    };
    auto verifySource = [&](int key) {
        const auto meta = metadata(key);
        if (sha256(resources.scenario(rom, key)) != meta["sha256"].toString().toLatin1())
            throw Error(
                QString("Scenario %1 differs from the browser project's source resource").arg(key));
    };
    // Early v4 files sometimes grouped equal bytes instead of actual resource
    // identities. A matching source hash permits the same canonical migration
    // as the browser reader; a mismatched hash never selects another resource.
    QMap<QString, QSet<QString>> migratedPaths;
    auto esets = object(project["modifiedEsets"], "scenario layouts", true);
    for (auto it = esets.begin(); it != esets.end(); ++it) {
        const int key = keyNumber(it.key());
        auto entry = object(it.value(), "scenario layout");
        fields(entry,
               {"runtimeKey", "archive", "filename", "resourcePath", "sourceSha256", "rawHex"},
               "scenario layout");
        const auto meta = metadata(key);
        const auto sourceHash = entry["sourceSha256"].toString().toLower();
        if (!sourceHash.isEmpty() && sourceHash != meta["sha256"].toString())
            throw Error("Imported scenario source hash does not match its runtime key");
        const auto path = entry["resourcePath"].toString();
        if (!path.isEmpty() && path != meta["resourcePath"].toString() &&
            sourceHash == meta["sha256"].toString()) {
            migratedPaths[path].insert(meta["resourcePath"].toString());
            entry["resourcePath"] = meta["resourcePath"];
            entry["filename"] = meta["filename"];
            entry["archive"] = meta["archive"];
        }
        if (entry.contains("runtimeKey") &&
            integer(entry["runtimeKey"], 1, 64, "runtime key") != key)
            throw Error("Imported layout and runtime key disagree");
        for (const auto field : {"resourcePath", "filename", "archive"})
            if (entry.contains(field) && entry[field] != meta[field])
                throw Error("Imported layout resource identity does not match its runtime key");
        if (strict && (sourceHash.isEmpty() || !entry.contains("resourcePath")))
            throw Error("Scenario project v4 requires source hashes and resource paths");
        verifySource(key);
        next.rows_[key].layout = Eset::decode(hex(entry["rawHex"], -1, strict));
        // Header byte 8 alone did not represent a level edit in browser projects.
        next.rows_[key].layout.setBaseLevel(next.original(key).layout.baseLevel());
        it.value() = entry;
    }
    QMap<QString, int> baseEdits;
    for (const auto &value : array(project["levelBaseEdits"], "base-level edits")) {
        const auto entry = object(value, "base-level edit");
        fields(entry, {"resourcePath", "original", "value"}, "base-level edit");
        const auto path = entry["resourcePath"].toString();
        const int oldLevel = integer(entry["original"], 1, 99, "original base level"),
                  level = integer(entry["value"], 1, 99, "base level");
        auto paths = migratedPaths.value(path);
        paths.insert(path);
        bool found = false;
        for (auto it = next.rows_.begin(); it != next.rows_.end(); ++it) {
            const auto meta = metadata(it.key());
            const auto actual = meta["resourcePath"].toString();
            if (!paths.contains(actual))
                continue;
            found = true;
            if (next.original(it.key()).layout.baseLevel() != oldLevel ||
                (baseEdits.contains(actual) && baseEdits[actual] != level))
                throw Error("Imported base-level edits conflict with their source or each other");
            verifySource(it.key());
            if (esets.contains(QString::number(it.key())) &&
                Eset::decode(hex(esets[QString::number(it.key())].toObject()["rawHex"], -1, strict))
                        .baseLevel() != level)
                throw Error("Imported layout and explicit base-level edit disagree");
            it->layout.setBaseLevel(level);
            baseEdits[actual] = level;
        }
        if (!found)
            throw Error("Imported base-level resource is unknown");
    }
    QMap<QString, QByteArray> records;
    auto claim = [&](int key, int edat, const QByteArray &record) {
        const auto identity = recordKey(key, edat);
        if (record.size() != 35 || (records.contains(identity) && records[identity] != record))
            throw Error("Conflicting imported squad record content");
        records[identity] = record;
    };
    for (auto it = overrides.begin(); it != overrides.end(); ++it) {
        const auto entry = object(it.value(), "squad override");
        fields(entry, {"scenario_id", "edat_id", "record", "original"}, "squad override");
        const auto parts = it.key().split(':');
        if (parts.size() != 2)
            throw Error("Imported squad key must identify scenario and template");
        const int key = keyNumber(parts[0]), edat = keyNumber(parts[1], 65534);
        next.scenario(key);
        if ((entry.contains("scenario_id") &&
             integer(entry["scenario_id"], 1, 64, "squad scenario") != key) ||
            (entry.contains("edat_id") &&
             integer(entry["edat_id"], 0, 65534, "squad template") != edat))
            throw Error("Imported squad key and identity disagree");
        if (entry.contains("original") &&
            hex(entry["original"], 35, false) != enemies_.mid(edat * 35, 35))
            throw Error("Imported squad override differs from its source template");
        claim(key, edat, hex(entry["record"], 35, false));
    }
    QMap<QString, QByteArray> owners;
    auto own = [&](int key, int row, int source, int edat, const QByteArray &record, bool added) {
        const auto &s = next.scenario(key);
        if (row < 0 || row >= s.layout.deployments.size() ||
            (u8(s.layout.deployments[row], 0) & 127) != source ||
            u16(s.layout.deployments[row], 1) != edat + 1)
            throw Error("Imported squad ownership does not match its deployment row");
        bool wasOriginal = false;
        for (const auto &d : next.original(key).layout.deployments)
            wasOriginal |= (u8(d, 0) & 127) == source;
        if (added && wasOriginal)
            throw Error("An added squad reuses an original deployment identity");
        const auto identity = recordKey(key, row);
        if (owners.contains(identity) && owners[identity] != record)
            throw Error("Conflicting imported deployment ownership");
        owners[identity] = record;
        claim(key, edat, record);
    };
    // Added records and explicit level copies are processed below before any
    // deployment resolves its template. This keeps import atomic across sections.
    for (const auto &value : array(project["addedSquads"], "added squads")) {
        const auto entry = object(value, "added squad");
        fields(entry,
               {"runtimeKey", "sourceId", "edatId", "section1Row", "status", "createdAt",
                "resourcePath", "identity", "originalLevelOffsetsRaw", "levelOffsetsRaw",
                "compRecHex"},
               "added squad");
        const int key = integer(entry["runtimeKey"], 1, 64, "added squad key"),
                  row = integer(entry["section1Row"], 0, 19, "added squad row"),
                  source = integer(entry["sourceId"], 30, 49, "added squad source"),
                  edat = integer(entry["edatId"], 0, 65534, "added squad template");
        const auto actualPath = metadata(key)["resourcePath"].toString();
        const auto path = entry["resourcePath"].toString();
        if ((strict || !path.isEmpty()) && path != actualPath &&
            !migratedPaths.value(path).contains(actualPath))
            throw Error("Imported added squad resource identity does not match its runtime key");
        const auto identity = object(entry["identity"], "added squad identity", !strict);
        fields(identity, {"kind", "runtimeKey", "sourceId", "section1Row"}, "added squad identity");
        if (!identity.isEmpty() &&
            (identity["kind"] != "added" || identity["runtimeKey"] != key ||
             identity["sourceId"] != source || identity["section1Row"] != row))
            throw Error("Imported added squad has conflicting nested identity");
        QByteArray record;
        if (entry.contains("compRecHex"))
            record = hex(entry["compRecHex"], 35, strict);
        else if (!strict)
            record = records.value(recordKey(key, edat), enemies_.mid(edat * 35, 35));
        else
            throw Error("Scenario project v4 requires complete added-squad records");
        offsets(entry["originalLevelOffsetsRaw"], QByteArray(35, 0), false);
        offsets(entry["levelOffsetsRaw"], record, false);
        own(key, row, source, edat, record, true);
    }
    for (const auto &value : array(project["squadLevelCopies"], "squad level copies")) {
        const auto entry = object(value, "squad level copy");
        fields(entry,
               {"runtimeKey", "resourcePath", "resourceSha256", "section1Row", "sourceId",
                "originalEdatId", "originalEdatOneBased", "originalRecordSha256",
                "originalRecordHex", "sourceRecordHex", "originalOffsetsRaw", "requestedOffsetsRaw",
                "customEdatId", "customEdatOneBased", "resourceAliasKeys", "customRecordHex",
                "aliasRecordHexByKey"},
               "squad level copy");
        const int key = integer(entry["runtimeKey"], 1, 64, "level-copy key"),
                  row = integer(entry["section1Row"], 0, 19, "level-copy row"),
                  source = integer(entry["sourceId"], 0, 49, "level-copy source"),
                  originalId = integer(entry["originalEdatId"], 0, 65534, "original template"),
                  customId = integer(entry["customEdatId"], 0, 65534, "copied template");
        const auto meta = metadata(key);
        verifySource(key);
        const auto &baseline = next.original(key);
        if (row >= baseline.layout.deployments.size() ||
            (u8(baseline.layout.deployments[row], 0) & 127) != source ||
            u16(baseline.layout.deployments[row], 1) != originalId + 1)
            throw Error("Imported level copy does not match its original deployment");
        const auto originalRecord = baseline.squads[row];
        if (entry["originalRecordSha256"].toString().toLower().toLatin1() != sha256(originalRecord))
            throw Error("Imported level copy has the wrong original squad hash");
        if (entry.contains("originalRecordHex") &&
            hex(entry["originalRecordHex"], 35, strict) != originalRecord)
            throw Error("Imported level copy has the wrong original squad bytes");
        if (entry.contains("sourceRecordHex"))
            hex(entry["sourceRecordHex"], 35, strict);
        if ((entry.contains("originalEdatOneBased") &&
             entry["originalEdatOneBased"] != originalId + 1) ||
            (entry.contains("customEdatOneBased") && entry["customEdatOneBased"] != customId + 1))
            throw Error("Imported level-copy template indices disagree");
        const auto hash = entry["resourceSha256"].toString().toLower();
        if (!hash.isEmpty() && hash != meta["sha256"].toString())
            throw Error("Imported level-copy resource hash differs from its source");
        const auto path = entry["resourcePath"].toString();
        if (path != meta["resourcePath"].toString() && hash != meta["sha256"].toString())
            throw Error("Imported level-copy resource identity is unresolved");
        const auto record = hex(entry["customRecordHex"], 35, strict);
        offsets(entry["originalOffsetsRaw"], originalRecord, false);
        offsets(entry["requestedOffsetsRaw"], record, true);
        auto expectedAliases = meta["aliases"].toArray();
        auto suppliedAliases = entry.contains("resourceAliasKeys")
                                   ? array(entry["resourceAliasKeys"], "resource aliases")
                                   : expectedAliases;
        QSet<int> supplied, expected;
        for (const auto &alias : suppliedAliases) {
            const int id = integer(alias, 1, 64, "resource alias");
            if (supplied.contains(id))
                throw Error("Duplicate imported resource alias");
            supplied.insert(id);
        }
        for (const auto &alias : expectedAliases)
            expected.insert(alias.toInt());
        if (supplied != expected && (hash != meta["sha256"].toString() || !supplied.contains(key)))
            throw Error("Imported level-copy resource aliases disagree with their source");
        const auto aliasRecords =
            object(entry["aliasRecordHexByKey"], "alias squad records", !strict);
        for (const int alias : expected) {
            metadata(alias);
            const auto aliasValue = aliasRecords[QString::number(alias)];
            QByteArray aliasRecord;
            if (!aliasValue.isUndefined())
                aliasRecord = hex(aliasValue, 35, strict);
            else if (!strict)
                aliasRecord = alias == key ? record : originalRecord;
            else
                throw Error("Imported level copy is missing a physical alias's squad record");
            if (alias == key && aliasRecord != record)
                throw Error("Imported level-copy and selected alias record disagree");
            own(alias, row, source, customId, aliasRecord, false);
        }
    }
    QSet<QString> consumed;
    for (auto it = next.rows_.begin(); it != next.rows_.end(); ++it) {
        auto &s = it.value();
        s.squads.clear();
        for (auto &row : s.layout.deployments) {
            const int edat = u16(row, 1) - 1;
            if (edat < 0)
                throw Error("Imported deployment has no squad template ID");
            const auto identity = recordKey(s.key, edat);
            QByteArray record = records.value(identity, enemies_.mid(edat * 35, 35));
            if (record.size() != 35)
                throw Error("Imported deployment has no complete squad record");
            if (records.contains(identity))
                consumed.insert(identity);
            s.squads << record;
            bool allied = automaticAlly(s.key, edat + 1);
            for (const auto &before : next.original(s.key).layout.deployments)
                if ((u8(before, 0) & 127) == (u8(row, 0) & 127))
                    allied |= (u8(before, 0) & 128) != 0;
            if (allied)
                row[0] = char(u8(row, 0) | 128);
        }
    }
    if (consumed.size() != records.size())
        throw Error("An imported squad override is not referenced by any imported deployment");
    next.validateForExport(classes);
    // Apply also canonicalizes framing and normalizes untouched originals.
    auto normalized = *this;
    normalized.apply(next.json(), classes);
    *this = std::move(normalized);
}
} // namespace lordly
