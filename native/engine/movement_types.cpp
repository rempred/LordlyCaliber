#include "engine/movement_types.h"
#include "engine/movement_runtime.h"
#include <QJsonArray>
#include <QSet>
#include <bit>
#include <cmath>
namespace lordly {
MovementTypes MovementTypes::load(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Movement types currently require US Rev 0");
    MovementTypes m;
    if (const auto placed = MovementPlacement::read(rom)) {
        m = fromJson(placed->model);
        m.installed_ = true;
    } else {
        const QStringList names{"",         "Sky",  "Plain", "Forest",
                                "Mountain", "Snow", "Marsh", "Immobile"};
        for (int id = 1; id <= 7; ++id) {
            MovementType t;
            t.name = names[id];
            t.behavior = id;
            for (int terrain = 0; terrain < terrainCount; ++terrain)
                t.costs << u16(rom.bytes(), 0x13fa58 + id * 64 + terrain * 2);
            m.entries_[id] = t;
        }
        for (int i = 0; i < 7; ++i)
            m.priority_ << u8(rom.bytes(), 0x5cebc + i);
        for (int i = 0; i < 16; ++i)
            m.curve_ << std::bit_cast<float>(u32(rom.bytes(), 0x13cd6c + i * 4));
    }
    m.validate();
    m.original_ = m.json();
    return m;
}
void MovementTypes::validate() const {
    if (entries_.size() < 7 || entries_.size() > maximumId || curve_.size() != 16 ||
        priority_.size() != entries_.size())
        throw Error("Invalid movement type table size");
    for (int id = 1; id <= 7; ++id)
        if (!entries_.contains(id))
            throw Error("Original movement types must be retained");
    QSet<int> ids;
    for (int id : priority_) {
        if (!entries_.contains(id) || ids.contains(id))
            throw Error("Movement priority must include each type once");
        ids.insert(id);
    }
    for (float v : curve_)
        if (!std::isfinite(v))
            throw Error("Movement terrain curve contains an invalid value");
    for (auto i = entries_.begin(); i != entries_.end(); ++i) {
        const auto &t = i.value();
        if (i.key() < 1 || i.key() > maximumId || t.name.trimmed().isEmpty() ||
            t.name.size() > 80 || t.baseSpeed < 1 || t.baseSpeed > 255 || t.behavior < 1 ||
            t.behavior > 7 || t.costs.size() != terrainCount)
            throw Error("Invalid movement type name, speed or behavior");
        for (int cost : t.costs)
            if (cost < 0 || cost > 65535)
                throw Error("Terrain cost exceeds its stored range");
    }
}
QJsonObject MovementTypes::json() const {
    QJsonObject types;
    QJsonArray priority, curve;
    for (auto i = entries_.begin(); i != entries_.end(); ++i) {
        QJsonArray costs;
        for (int c : i->costs)
            costs.append(c);
        types[QString::number(i.key())] = QJsonObject{{"name", i->name},
                                                      {"baseSpeed", i->baseSpeed},
                                                      {"behavior", i->behavior},
                                                      {"costs", costs}};
    }
    for (int id : priority_)
        priority.append(id);
    for (float v : curve_)
        curve.append(double(v));
    return {{"version", 1}, {"types", types}, {"priority", priority}, {"curve", curve}};
}
MovementTypes MovementTypes::fromJson(const QJsonObject &j) {
    if (j["version"].toInt() != 1 || !j["types"].isObject() || !j["priority"].isArray() ||
        !j["curve"].isArray())
        throw Error("Invalid movement types document");
    auto integer = [](const QJsonValue &v) {
        if (!v.isDouble() || v.toDouble() != v.toInt(-999))
            throw Error("Movement values must be integers");
        return v.toInt();
    };
    MovementTypes m;
    const auto types = j["types"].toObject();
    for (auto i = types.begin(); i != types.end(); ++i) {
        bool ok = false;
        const int id = i.key().toInt(&ok);
        if (!ok || QString::number(id) != i.key() || !i->isObject())
            throw Error("Invalid movement type ID");
        const auto o = i->toObject();
        MovementType t;
        t.name = o["name"].toString();
        t.baseSpeed = integer(o["baseSpeed"]);
        t.behavior = integer(o["behavior"]);
        if (!o["costs"].isArray())
            throw Error("Missing movement terrain costs");
        for (const auto c : o["costs"].toArray())
            t.costs << integer(c);
        m.entries_[id] = t;
    }
    for (const auto v : j["priority"].toArray())
        m.priority_ << integer(v);
    for (const auto v : j["curve"].toArray()) {
        if (!v.isDouble())
            throw Error("Invalid terrain curve");
        m.curve_ << float(v.toDouble());
    }
    m.validate();
    return m;
}
void MovementTypes::apply(const QJsonObject &j) {
    auto m = fromJson(j);
    m.original_ = original_;
    m.installed_ = installed_;
    *this = std::move(m);
}
QJsonObject MovementTypes::labels() const {
    QJsonObject out;
    for (auto i = entries_.begin(); i != entries_.end(); ++i)
        out[QString::number(i.key())] = i->name;
    return out;
}
QByteArray MovementTypes::priorityBytes() const {
    QByteArray out;
    for (int id : priority_)
        out.append(char(id));
    return out;
}
int MovementTypes::create(const QString &name, int copyFrom) {
    if (!entries_.contains(copyFrom))
        throw Error("Choose a movement type to copy");
    int id = 8;
    while (entries_.contains(id))
        ++id;
    if (id > maximumId)
        throw Error("The movement type table is full");
    auto t = entries_[copyFrom];
    t.name = name.trimmed();
    auto next = *this;
    next.entries_[id] = t;
    next.priority_.insert(next.priority_.indexOf(copyFrom) + 1, id);
    next.validate();
    *this = std::move(next);
    return id;
}
void MovementTypes::remove(int id) {
    if (id <= 7 || !entries_.contains(id))
        throw Error("Only custom movement types can be deleted");
    entries_.remove(id);
    priority_.removeAll(id);
}
void MovementTypes::set(int id, const MovementType &t) {
    if (!entries_.contains(id))
        throw Error("Unknown movement type");
    auto next = *this;
    next.entries_[id] = t;
    next.validate();
    *this = std::move(next);
}
void MovementTypes::setPriority(const QVector<int> &p) {
    auto next = *this;
    next.priority_ = p;
    next.validate();
    *this = std::move(next);
}
int MovementTypes::behavior(int id) const {
    return entries_.contains(id) ? entries_[id].behavior : 7;
}
double MovementTypes::speed(int id, int terrain) const {
    if (!entries_.contains(id) || terrain < 0 || terrain >= 32)
        throw Error("Unknown movement type or terrain");
    const auto &t = entries_[id];
    int c = t.costs[terrain];
    if (c == 65535 || t.behavior == 7)
        return 0;
    return qMax(1., double(t.baseSpeed) - 2. * (c > 0 && c < 16 ? curve_[c] : 2.f));
}
} // namespace lordly
