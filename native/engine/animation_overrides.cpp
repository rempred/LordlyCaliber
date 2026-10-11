#include "engine/animation_overrides.h"
#include "engine/actor_art.h"
#include <QSet>
#include <algorithm>
#include <tuple>
namespace lordly {
namespace {
int integer(const QJsonObject &object, const QString &field, int min, int max) {
    const int value = object[field].toInt(min - 1);
    if (value < min || value > max)
        throw Error("Invalid animation override " + field);
    return value;
}
int owner(const ClassData &classes, int id) {
    const int value = u8(classes.entries()[id].record, 69);
    return value ? value : id;
}
QSet<int> actions() {
    QSet<int> result;
    for (const auto &value : AnimationOverrideImage::definition()["actions"].toArray())
        result.insert(value.toObject()["id"].toInt());
    return result;
}
QJsonObject logical(const AnimationOverrideImage::Row &row) {
    QJsonObject result{{"classId", row[0]},
                       {"actionId", row[2]},
                       {"normalSelector", row[5]},
                       {"blockedSelector", row[6]}};
    if (row[3] & 2)
        result["bodyFlags"] = row[7];
    return result;
}
auto logicalKey(const QJsonObject &row) {
    return std::tuple(row["classId"].toInt(), row["actionId"].toInt(),
                      row.contains("bodyFlags") ? row["bodyFlags"].toInt() : -1);
}
QByteArray rawTable(const QVector<AnimationOverrideImage::Row> &rows) {
    QByteArray table(1024, 0);
    for (int i = 0; i < rows.size(); ++i)
        for (int j = 0; j < 8; ++j)
            put8(table, i * 8 + j, rows[i][j]);
    return table;
}
} // namespace
QVector<int> AnimationOverrides::selectors(const Rom &rom, const AnimationRoutes &routes, int id,
                                           std::optional<int> bodyFlags) {
    if (bodyFlags && (*bodyFlags < 0 || *bodyFlags > 3))
        throw Error("Body flags must be in 0..3");
    const auto entry = routes.entry(id);
    QVector<int> result;
    QMap<quint16, ActorArt> assets;
    for (int flags = 0; flags < 4; ++flags)
        if (!bodyFlags || *bodyFlags == flags) {
            const auto handle = entry.handles[flags];
            if (!assets.contains(handle))
                assets.insert(handle, ActorArt::loadDirect(rom, handle));
        }
    // Combat's explicit-selector lane precedes the separate secondary-body lane.
    // A count-zero physical program is empty even though it has a one-byte source.
    for (int selector = 40; selector < 50; ++selector) {
        bool present = true;
        for (const auto &art : assets)
            present &= selector < art.poses().size() && !art.poses()[selector].records.isEmpty();
        if (present)
            result << selector;
    }
    return result;
}
AnimationOverrides AnimationOverrides::load(const Rom &rom, const ClassData &classes,
                                            const AnimationRoutes &routes) {
    AnimationOverrides result;
    result.image_ = AnimationOverrideImage::inspect(rom);
    if (!result.image_.recognized()) {
        result.reason_ = result.image_.reason();
        return result;
    }
    result.originalTable_ = rawTable(result.image_.rows());
    try {
        const auto accepted = actions();
        QVector<QJsonObject> rows;
        for (const auto &row : result.image_.rows()) {
            if (row[0] < 1 || row[0] >= classes.entries().size() ||
                row[1] != owner(classes, row[0]) || (row[3] & 1) || !accepted.contains(row[2]))
                throw Error("Advanced animation override records are preserved read-only");
            if (!(row[3] & 2)) {
                const auto options = selectors(rom, routes, row[0]);
                if (!options.contains(row[5]) || !options.contains(row[6]))
                    throw Error("Animation override selectors require an advanced or unavailable "
                                "body route; records are preserved read-only");
            }
            rows << logical(row);
        }
        std::sort(rows.begin(), rows.end(),
                  [](const auto &a, const auto &b) { return logicalKey(a) < logicalKey(b); });
        for (const auto &row : rows)
            result.entries_ << row;
        result.original_ = result.entries_;
    } catch (const Error &e) {
        result.reason_ = QString::fromUtf8(e.what());
        result.entries_ = {};
        result.original_ = {};
    }
    return result;
}
QJsonObject AnimationOverrides::json() const {
    return {{"version", 1},
            {"sourceSha256", QString::fromLatin1(image_.identity())},
            {"entries", entries_}};
}
void AnimationOverrides::setEntries(const QJsonArray &entries, const Rom &rom,
                                    const ClassData &classes, const AnimationRoutes &routes) {
    if (!available())
        throw Error(reason_);
    if (entries.size() > 128)
        throw Error("Animation override table is limited to 128 entries");
    const auto accepted = actions();
    QSet<QString> seen;
    QVector<QJsonObject> next;
    QMap<int, QVector<int>> options;
    for (const auto &value : entries) {
        if (!value.isObject())
            throw Error("Invalid animation override row");
        const auto row = value.toObject();
        for (auto it = row.begin(); it != row.end(); ++it)
            if (!QSet<QString>{"classId", "actionId", "normalSelector", "blockedSelector",
                               "bodyFlags"}
                     .contains(it.key()))
                throw Error("Unknown animation override field: " + it.key());
        const int id = integer(row, "classId", 1, std::min(255, int(classes.entries().size()) - 1)),
                  action = integer(row, "actionId", 0, 255),
                  normal = integer(row, "normalSelector", 0, 255),
                  blocked = integer(row, "blockedSelector", 0, 255);
        if (!accepted.contains(action))
            throw Error("This action has no supported combat override consumer");
        const int flags = row.contains("bodyFlags") ? integer(row, "bodyFlags", 0, 3) : -1;
        const auto key = QString("%1:%2:%3").arg(id).arg(action).arg(flags);
        if (seen.contains(key))
            throw Error("Duplicate class/action/body animation override");
        seen.insert(key);
        routes.entry(id); // Also rejects a pending class animation assignment.
        if (flags < 0) {
            if (!options.contains(id))
                options[id] = selectors(rom, routes, id);
            if (!options[id].contains(normal) || !options[id].contains(blocked))
                throw Error(
                    QString(
                        "Class %1 needs a selector with a nonempty pose in all four body routes")
                        .arg(id));
        }
        QJsonObject clean{{"classId", id},
                          {"actionId", action},
                          {"normalSelector", normal},
                          {"blockedSelector", blocked}};
        if (flags >= 0)
            clean["bodyFlags"] = flags;
        next << clean;
    }
    std::sort(next.begin(), next.end(),
              [](const auto &a, const auto &b) { return logicalKey(a) < logicalKey(b); });
    QJsonArray result;
    for (const auto &row : next)
        result << row;
    entries_ = std::move(result);
}
void AnimationOverrides::apply(const QJsonObject &object, const Rom &rom, const ClassData &classes,
                               const AnimationRoutes &routes) {
    if (object["version"] != 1 ||
        object["sourceSha256"].toString().toLatin1() != image_.identity() ||
        !object["entries"].isArray())
        throw Error("Saved animation overrides do not match their source ROM");
    setEntries(object["entries"].toArray(), rom, classes, routes);
}
void AnimationOverrides::importLegacy(const QJsonObject &object, int version, const Rom &rom,
                                      const ClassData &classes, const AnimationRoutes &routes) {
    if (version < 14 || object.size() != 1 || !object["entries"].isArray())
        throw Error("Invalid browser combat animation overrides");
    auto rows = object["entries"].toArray();
    if (version == 14)
        for (int i = 0; i < rows.size(); ++i) {
            auto row = rows[i].toObject();
            if (row.size() != 3 || !row.contains("classId") || !row.contains("actionId") ||
                !row.contains("selector"))
                throw Error("Invalid version-14 animation override");
            const int selector = integer(row, "selector", 0, 255);
            row.remove("selector");
            row["normalSelector"] = selector;
            row["blockedSelector"] = selector;
            rows[i] = row;
        }
    setEntries(rows, rom, classes, routes);
}
QByteArray AnimationOverrides::table(const ClassData &classes) const {
    if (!available())
        throw Error(reason_);
    QVector<AnimationOverrideImage::Row> rows;
    for (const auto &value : entries_) {
        const auto entry = value.toObject();
        const int id = entry["classId"].toInt();
        if (id < 1 || id >= classes.entries().size())
            throw Error("Animation override class is no longer present");
        const bool exact = entry.contains("bodyFlags");
        rows << AnimationOverrideImage::Row{quint8(id),
                                            quint8(owner(classes, id)),
                                            quint8(entry["actionId"].toInt()),
                                            quint8(exact ? 2 : 0),
                                            0,
                                            quint8(entry["normalSelector"].toInt()),
                                            quint8(entry["blockedSelector"].toInt()),
                                            quint8(exact ? entry["bodyFlags"].toInt() : 0)};
    }
    auto key = [](const auto &row) {
        return std::tuple(row[0], row[1], row[2], row[3] ? 0 : 1, row[7], row[4]);
    };
    std::sort(rows.begin(), rows.end(),
              [&](const auto &a, const auto &b) { return key(a) < key(b); });
    return rawTable(rows);
}
bool AnimationOverrides::needsRebuild(const ClassData &classes) const {
    return available() && (modified() || table(classes) != originalTable_);
}
QByteArray AnimationOverrides::moduleData(const ClassData &classes) const {
    const auto records = table(classes);
    QByteArray bytes(0x800, 0);
    bytes.replace(0, 4, "OBSO");
    const std::array<quint16, 6> halves{3, 0x40, 8, 128, quint16(entries_.size()), 0};
    for (int i = 0; i < 6; ++i)
        put16(bytes, 4 + i * 2, halves[i]);
    const std::array<quint32, 4> words{0x800, 0x40, 0x400, 1};
    for (int i = 0; i < 4; ++i)
        put32(bytes, 0x10 + i * 4, words[i]);
    bytes.replace(0x20, 32, QByteArray::fromHex(sha256(records)));
    bytes.replace(0x400, 1024, records);
    return bytes;
}
} // namespace lordly
