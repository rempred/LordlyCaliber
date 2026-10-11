#include "engine/scenario_world.h"
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <cmath>

namespace lordly {
namespace {
int integer(const QJsonValue &value, int lo, int hi) {
    const double n = value.toDouble(-1);
    if (!value.isDouble() || !std::isfinite(n) || n != std::floor(n) || n < lo || n > hi)
        throw Error("Imported world field is outside its integer range");
    return int(n);
}
int key(const QString &text, int lo, int hi) {
    bool ok = false;
    const int n = text.toInt(&ok);
    if (!ok || QString::number(n) != text || n < lo || n > hi)
        throw Error("Imported world record key is invalid");
    return n;
}
QJsonObject object(const QJsonValue &v, bool optional = false) {
    if (optional && (v.isUndefined() || v.isNull()))
        return {};
    if (!v.isObject())
        throw Error("Imported world section must be an object");
    return v.toObject();
}
void fields(const QJsonObject &o, const QSet<QString> &allowed) {
    for (auto i = o.begin(); i != o.end(); ++i)
        if (!allowed.contains(i.key()))
            throw Error("Unknown imported world field: " + i.key());
}
} // namespace
void ScenarioWorld::importBrowser(const Rom &rom, const QJsonObject &input) {
    if (input.isEmpty())
        return;
    const int version = integer(input["version"], 1, 4);
    if (input["format"] != "ob64-scenario-project")
        throw Error("Unsupported browser world project");
    const auto treasureEdits = object(input["modifiedTreasures"], true),
               allegiances = object(input["siteAllegiances"], true),
               townEdits = object(input["strongholdFields"], true);
    if (treasureEdits.isEmpty() && allegiances.isEmpty() && townEdits.isEmpty())
        return;
    if (modified())
        throw Error("Import browser world data before making native site or treasure edits");
    QFile file(":/game/scenarios/world-import-catalog.json");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing browser world resource identities");
    const auto catalog = QJsonDocument::fromJson(file.readAll()).object();
    const auto treasureCatalog = catalog["treasures"].toObject(),
               siteCatalog = catalog["sites"].toObject(),
               scenarios = catalog["scenarios"].toObject();
    const auto resources = WorldResources::load(rom);
    auto next = *this;
    if (!townEdits.isEmpty() &&
        sha256(originalTowns_) != catalog["townsSha256"].toString().toLatin1())
        throw Error("Site table differs from the browser project's source");
    for (auto i = townEdits.begin(); i != townEdits.end(); ++i) {
        const int id = key(i.key(), 0, townCount - 1) + 1;
        const auto edit = object(i.value());
        fields(edit, {"population", "morale"});
        auto record = next.town(id);
        if (edit.contains("population"))
            put16(record, 22, quint16(integer(edit["population"], 0, 65535)));
        if (edit.contains("morale")) {
            const int morale = integer(edit["morale"], 0, 127);
            if (u8(record, 24) == 255)
                throw Error("Browser morale edits cannot replace an objective marker");
            put8(record, 24, quint8(morale));
        }
        next.setTown(id, record);
    }
    for (auto i = treasureEdits.begin(); i != treasureEdits.end(); ++i) {
        const int archive = key(i.key(), 648, 687);
        const auto edit = object(i.value()), meta = treasureCatalog[i.key()].toObject();
        fields(edit, {"archive", "filename", "rawHex"});
        if ((edit.contains("archive") && integer(edit["archive"], 648, 687) != archive) ||
            (edit.contains("filename") && edit["filename"] != meta["filename"]))
            throw Error("Imported treasure archive identity mismatch");
        if (!edit["rawHex"].isString())
            throw Error("Imported treasure payload must be hexadecimal");
        auto text = edit["rawHex"].toString();
        if (version < 4)
            text.remove(QRegularExpression("\\s+"));
        const auto raw = QByteArray::fromHex(text.toLatin1());
        if (raw.isEmpty() || raw.toHex() != text.toLatin1().toLower() ||
            raw.size() != 1 + u8(raw, 0) * 6)
            throw Error("Imported treasure payload has invalid framing");
        bool found = false;
        for (auto s = scenarios.begin(); s != scenarios.end(); ++s) {
            if (s.value().toObject()["treasureArchive"].toInt() != archive)
                continue;
            const int k = key(s.key(), 1, 64);
            if (resources.treasureSelector(k) != meta["member"].toInt() ||
                sha256(original(k).treasures) != meta["sha256"].toString().toLatin1())
                throw Error("Treasure source differs from the browser archive");
            next.rows_[k].treasures = raw;
            found = true;
        }
        if (!found)
            throw Error("Imported treasure archive has no playable scenario consumer");
    }
    QMap<QPair<int, int>, int> claims;
    for (auto i = allegiances.begin(); i != allegiances.end(); ++i) {
        const int k = key(i.key(), 1, 64);
        const auto meta = scenarios[i.key()].toObject();
        const auto edits = object(i.value()), selectors = meta["selectors"].toObject();
        for (auto e = edits.begin(); e != edits.end(); ++e) {
            key(e.key(), 0, 255);
            const auto selector = selectors[e.key()].toObject();
            const int allegiance = e.value() == "neutral"  ? 0
                                   : e.value() == "allied" ? 1
                                   : e.value() == "enemy"  ? 2
                                                           : -1;
            if (allegiance < 0 || !selector["present"].toBool())
                throw Error(
                    "Imported allegiance lacks a supported site descriptor or target state");
            const int archive = meta["siteArchive"].toInt(), row = selector["row"].toInt(-1);
            const auto resource = siteCatalog[QString::number(archive)].toObject();
            if (resource.isEmpty() || resources.siteSelector(k) != resource["member"].toInt() ||
                sha256(original(k).sites) != resource["sha256"].toString().toLatin1() || row < 0 ||
                row >= declaredSiteCount(original(k).sites) ||
                u16(original(k).sites, 1 + row * 4) != selector["siteId"].toInt())
                throw Error("Imported allegiance does not match the source site's identity");
            const auto identity = qMakePair(archive, row);
            const int targetAddend = allegiance == 1 ? 0x2012 : allegiance == 2 ? 4 : 0;
            if (u16(original(k).sites, 3 + row * 4) == targetAddend)
                continue; // Browser export ignores intents equal to their source value.
            if (claims.contains(identity) && claims[identity] != allegiance)
                throw Error("Browser allegiance intents conflict on a shared site resource");
            claims[identity] = allegiance;
        }
    }
    for (auto c = claims.begin(); c != claims.end(); ++c) {
        const auto resource = siteCatalog[QString::number(c.key().first)].toObject();
        for (auto s = scenarios.begin(); s != scenarios.end(); ++s) {
            if (s.value().toObject()["siteArchive"].toInt() != c.key().first)
                continue;
            const int k = key(s.key(), 1, 64);
            if (resources.siteSelector(k) != resource["member"].toInt() ||
                sha256(original(k).sites) != resource["sha256"].toString().toLatin1())
                throw Error("A shared site source differs from the browser project");
            next.setSiteAllegiance(k, c.key().second, c.value());
        }
    }
    apply(next.json()); // Validate all flags and records atomically, including shared aliases.
}
} // namespace lordly
