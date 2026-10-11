#include "engine/scene_events.h"
#include "engine/legacy_scene_arena.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
namespace lordly {
namespace {
const QJsonObject &catalog() {
    static const QJsonObject value = [] {
        QFile file(":/game/director/events.json");
        if (!file.open(QIODevice::ReadOnly))
            throw Error("Missing event launch catalog");
        const auto object = QJsonDocument::fromJson(file.readAll()).object();
        if (object["format"] != "lordly-event-sites" || object["version"] != 1 ||
            object["rows"].toArray().size() != 115 || object["sites"].toArray().size() != 1998)
            throw Error("Invalid event launch catalog");
        return object;
    }();
    return value;
}
QJsonObject rowInfo(int row) {
    const auto rows = catalog()["rows"].toArray();
    if (row < 0 || row >= rows.size() || !rows[row].isObject())
        throw Error("Event row is absent from the catalog");
    return rows[row].toObject();
}
QByteArray decode(const Rom &rom, quint32 key) {
    const quint64 at = quint64(key) + SceneEvents::base;
    if (!key || key >= 0x10000000 || at + 8 > quint64(rom.bytes().size()))
        throw Error("Event row key is outside the ROM");
    const auto size = u32(rom.bytes(), at), decoded = u32(rom.bytes(), at + 4);
    if (size < 4 || size > 1024 * 1024 || !decoded || decoded > 131070 || decoded % 2 ||
        at + 4 + size > quint64(rom.bytes().size()))
        throw Error("Event row exceeds its resource or cursor bounds");
    return decodeLzss(rom.bytes().mid(at + 8, size - 4), decoded);
}
} // namespace
const QMap<QString, SceneEventSite> &SceneEvents::sites() {
    static const QMap<QString, SceneEventSite> result = [] {
        QMap<QString, SceneEventSite> rows;
        for (const auto value : catalog()["sites"].toArray()) {
            const auto item = value.toObject();
            SceneEventSite site{item["launchSiteId"].toString(),  item["name"].toString(),
                                item["role"].toString(),          item["mission"].toString(),
                                item["retailSceneId"].toString(), item["row"].toInt(),
                                item["offset"].toInt(),           item["selector"].toInt(),
                                item["entryCursor"].toInt(),      item["form"] == "short",
                                item["shared"].toBool()};
            if (site.id.isEmpty() || rows.contains(site.id) || site.row < 0 || site.row >= 115 ||
                site.offset < (site.shortForm ? 2 : 4) || site.offset % 2 || site.selector < 0 ||
                site.selector > 65535)
                throw Error("Invalid or duplicated event launch identity");
            rows[site.id] = site;
        }
        return rows;
    }();
    return result;
}
const SceneEventSite &SceneEvents::site(const QString &id) {
    const auto found = sites().constFind(id);
    if (found == sites().cend())
        throw Error("Event launch site is not in the catalog");
    return found.value();
}
const QVector<SceneEventContext> &SceneEvents::contexts() {
    static const QVector<SceneEventContext> result = [] {
        QVector<SceneEventContext> result;
        for (const auto &v : catalog()["contexts"].toArray()) {
            const auto row = v.toObject();
            SceneEventContext context;
            context.id = row["id"].toString();
            context.site = row["site"].toString();
            context.parent = row["parent"].toString();
            context.invocation = row["invocation"].toInt(-1);
            context.parentInvocation = row["parentInvocation"].toInt(-1);
            context.ticks = qMax(1, row["ticks"].toInt(1));
            const auto translations = row["translations"].toArray();
            for (int i = 0; i < translations.size() && i < 256; ++i)
                if (translations[i].isDouble())
                    context.translations.set(i, quint16(translations[i].toInt()));
            result << context;
        }
        return result;
    }();
    return result;
}
quint32 SceneEvents::retailKey(int row) {
    return quint32(rowInfo(row)["key"].toInteger());
}
QByteArray SceneEvents::retailRow(const Rom &rom, int row) {
    const auto info = rowInfo(row);
    const auto key = retailKey(row);
    if (u32(rom.bytes(), base + key) != quint32(info["storedLength"].toInteger()))
        throw Error("The preserved event preimage has changed");
    const auto bytes = decode(rom, key);
    if (bytes.size() != info["decodedLength"].toInt() ||
        sha256(bytes).toUpper() != info["decodedSha256"].toString().toLatin1())
        throw Error("The preserved event preimage differs from the audited program");
    return bytes;
}
QByteArray SceneEvents::readRow(const Rom &rom, int row) {
    if (row < 0 || row >= 115 || u32(rom.bytes(), directory) != 115 * 4)
        throw Error("Invalid event directory row");
    return decode(rom, u32(rom.bytes(), directory + 4 + row * 4));
}
QByteArray SceneEvents::bindRow(const QByteArray &retail, int row,
                                const QMap<QString, int> &bindings,
                                const QSet<QString> &forceStubs) {
    auto output = retail;
    if (retail.size() % 2)
        throw Error("Event program must contain complete word instructions");
    for (auto it = bindings.cbegin(); it != bindings.cend(); ++it) {
        const auto &entry = site(it.key());
        if (entry.row != row)
            continue;
        const auto at = entry.offset, selector = it.value();
        if (selector < 0 || selector > 65535)
            throw Error("Scene selector exceeds the event operand");
        if (entry.shortForm) {
            if (u8(retail, at - 2) != 0x68 || u8(retail, at - 1) != entry.selector ||
                u8(retail, at) != 0x10)
                throw Error("Short launch no longer matches its audited instruction sequence");
            if (selector <= 255 && !forceStubs.contains(entry.id)) {
                put8(output, at - 1, quint8(selector));
                continue;
            }
            const auto stub = output.size();
            if (stub + 10 > 131070)
                throw Error("Event launch stub exceeds the sixteen-bit cursor");
            const auto out = quint16((stub - (at + 2)) / 2),
                       back = quint16((at + 2 - (stub + 10)) / 2);
            output.replace(at - 2, 4, QByteArray::fromHex("68000200"));
            put8(output, at - 1, quint8(out));
            put8(output, at + 1, quint8(out >> 8));
            QByteArray code = QByteArray::fromHex("68008800100068000200");
            put8(code, 1, quint8(selector));
            put8(code, 3, quint8(selector >> 8));
            put8(code, 5, u8(retail, at + 1));
            put8(code, 7, quint8(back));
            put8(code, 9, quint8(back >> 8));
            output += code;
        } else {
            if (u8(retail, at - 4) != 0x68 || u8(retail, at - 3) != (entry.selector & 255) ||
                u8(retail, at - 2) != 0x88 || u8(retail, at - 1) != (entry.selector >> 8) ||
                u8(retail, at) != 0x10 || u8(retail, at + 1) != 0)
                throw Error("Long launch no longer matches its audited instruction sequence");
            put8(output, at - 3, quint8(selector));
            put8(output, at - 1, quint8(selector >> 8));
        }
    }
    return output;
}
QMap<QString, int> SceneEvents::discoverRow(const QByteArray &retail, const QByteArray &current,
                                            int row) {
    QMap<QString, int> bindings;
    QSet<QString> stubs;
    for (const auto &entry : sites()) {
        if (entry.row != row)
            continue;
        const auto at = entry.offset;
        int selector;
        if (!entry.shortForm)
            selector = u8(current, at - 3) | (int(u8(current, at - 1)) << 8);
        else if (u8(current, at) == 0x10)
            selector = u8(current, at - 1);
        else {
            if (u8(current, at - 2) != 0x68 || u8(current, at) != 2)
                throw Error("Unknown event launch rewrite");
            const auto relative = quint16(u8(current, at - 1) | (int(u8(current, at + 1)) << 8));
            const auto stub = int(quint16((at + 2) / 2 + relative)) * 2;
            if (stub < retail.size() || stub + 10 > current.size())
                throw Error("Event launch stub is outside its allocation");
            stubs.insert(entry.id);
            selector = u8(current, stub + 1) | (int(u8(current, stub + 3)) << 8);
        }
        if (selector != entry.selector || stubs.contains(entry.id))
            bindings[entry.id] = selector;
    }
    if (bindRow(retail, row, bindings, stubs) != current)
        throw Error("Event program contains edits outside supported launch bindings");
    for (auto it = bindings.begin(); it != bindings.end();) {
        if (it.value() == site(it.key()).selector)
            it = bindings.erase(it);
        else
            ++it;
    }
    return bindings;
}
SceneEvents SceneEvents::load(Rom rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Event bindings require US revision zero");
    if (u32(rom.bytes(), directory) != 115 * 4)
        throw Error("Event directory differs from the supported layout");
    SceneEvents result(rom);
    const auto placement = SceneProgramResources::placement(rom);
    if (!placement || !placement->legacyArenaRetired)
        LegacySceneArena::read(rom);
    if (placement)
        result.bindings_ = placement->eventBindings;
    for (int row = 0; row < 115; ++row) {
        const auto info = catalog()["rows"].toArray()[row];
        const auto key = u32(rom.bytes(), directory + 4 + row * 4);
        if (key == quint32(info.toObject()["key"].toInteger()))
            continue;
        if (placement && placement->eventRows.contains(row))
            continue;
        const auto found = discoverRow(retailRow(rom, row), readRow(rom, row), row);
        for (auto it = found.cbegin(); it != found.cend(); ++it)
            result.bindings_[it.key()] = it.value();
    }
    result.original_ = result.bindings_;
    return result;
}
int SceneEvents::target(const QString &id) const {
    return bindings_.value(id, site(id).selector);
}
void SceneEvents::bind(const QString &id, int selector, const ScenePrograms &scenes) {
    const auto &entry = site(id);
    auto next = *this;
    if (selector == entry.selector)
        next.bindings_.remove(id);
    else
        next.bindings_[id] = selector;
    next.validate(scenes);
    *this = std::move(next);
}
void SceneEvents::restore(const QString &id) {
    site(id);
    bindings_.remove(id);
}
void SceneEvents::validate(const ScenePrograms &scenes) const {
    QSet<int> selectors;
    for (const auto &id : scenes.scenes())
        for (const auto selector : scenes.resource(id).selectors)
            selectors.insert(selector);
    for (auto it = bindings_.cbegin(); it != bindings_.cend(); ++it) {
        site(it.key());
        if (!selectors.contains(it.value()))
            throw Error(QString("Launch %1 targets an absent scene selector %2")
                            .arg(it.key())
                            .arg(it.value()));
    }
}
QJsonObject SceneEvents::json() const {
    QJsonObject bindings;
    for (auto it = bindings_.cbegin(); it != bindings_.cend(); ++it)
        bindings[it.key()] = it.value();
    return {{"version", 1}, {"bindings", bindings}};
}
void SceneEvents::apply(const QJsonObject &value, const ScenePrograms &scenes) {
    if (value["version"] != 1 || !value["bindings"].isObject())
        throw Error("Invalid saved event bindings");
    auto next = *this;
    next.bindings_.clear();
    const auto values = value["bindings"].toObject();
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        const auto selector = it.value().toInt(-1);
        if (selector < 0 || selector > 65535 || it.value().toDouble() != selector)
            throw Error("Invalid saved event target selector");
        if (selector != site(it.key()).selector)
            next.bindings_[it.key()] = selector;
    }
    next.validate(scenes);
    *this = std::move(next);
}
SceneEventBuild SceneEvents::plan() const {
    SceneEventBuild result;
    result.bindings = bindings_;
    for (auto it = bindings_.cbegin(); it != bindings_.cend(); ++it) {
        const auto row = site(it.key()).row;
        if (!result.rows.contains(row))
            result.rows[row] = bindRow(retailRow(rom_, row), row, bindings_);
    }
    return result;
}
int SceneEvents::changedSites() const {
    QSet<QString> keys;
    for (auto it = bindings_.cbegin(); it != bindings_.cend(); ++it)
        keys.insert(it.key());
    for (auto it = original_.cbegin(); it != original_.cend(); ++it)
        keys.insert(it.key());
    int count = 0;
    for (const auto &id : keys)
        count += bindings_.value(id, site(id).selector) != original_.value(id, site(id).selector);
    return count;
}
} // namespace lordly
