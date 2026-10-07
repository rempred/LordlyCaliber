#include "engine/shops.h"
#include "engine/shop_runtime.h"
#include "engine/world_resources.h"
#include <QJsonArray>
#include <QSet>
#include <cmath>

namespace lordly {
void Shops::validate(int id, const ShopStock &stock) {
    if (id < 0 || id >= 256 ||
        (id == 0 && (!stock.equipment.isEmpty() || !stock.consumables.isEmpty())))
        throw Error("Shop 0 must remain empty; editable shop IDs are 1 through 255");
    auto check = [](const QVector<quint16> &ids, int capacity, int maximum, const QString &kind) {
        if (ids.size() > capacity)
            throw Error(QString("A shop supports at most %1 %2").arg(capacity).arg(kind));
        QSet<quint16> unique;
        for (const auto id : ids) {
            if (!id || id > maximum || unique.contains(id))
                throw Error("Shop " + kind + " contain an invalid or duplicate ID");
            unique.insert(id);
        }
    };
    check(stock.equipment, equipmentCapacity, 510, "equipment entries");
    check(stock.consumables, consumableCapacity, 44, "consumables");
}
Shops Shops::load(const Rom &rom) {
    rom.requireSupported();
    const auto placement = ShopPlacement::read(rom);
    if (!placement)
        verifyShopRuntime(rom, false);
    Shops result;
    const auto world =
        rom.revision() == 0 ? std::optional(WorldResources::load(rom)) : std::nullopt;
    const auto data =
        world ? world->shopInventory(rom) : LhaMember::find(rom, "shopcsv.bin").decode(rom);
    if (data.size() < 512 || u16(data, 0) != 512 || u16(data, 2) != 512 ||
        u16(data, count * 2) != data.size())
        throw Error("Unrecognized shop archive layout");
    for (int at = (count + 1) * 2; at < 512; ++at)
        if (u8(data, at))
            throw Error("Shop archive contains unowned additional records");
    for (int id = 0; id < count; ++id) {
        const auto begin = u16(data, id * 2), end = u16(data, (id + 1) * 2);
        if (begin < 512 || begin > end || end > data.size() || (begin & 1) || (end & 1))
            throw Error("Shop inventory exceeds its archive allocation");
        ShopStock stock;
        for (int at = begin; at < end; at += 2)
            stock.equipment.push_back(u16(data, at));
        if (id)
            stock.consumables = {1, 2, 3, 4, 5, 6, 8};
        validate(id, stock);
        result.stock_.push_back(stock);
    }
    if (placement) {
        result.stock_.resize(placement->count);
        for (int id = 0; id < placement->count; ++id) {
            const auto row = rom.bytes().mid(placement->offset + id * ShopPlacement::rowSize,
                                             ShopPlacement::rowSize);
            ShopStock stock;
            bool ended = false, equipment = false;
            for (int at = 0; at < row.size(); at += 2) {
                const auto word = u16(row, at);
                if (!word) {
                    ended = true;
                    continue;
                }
                if (ended || (!(word & 0x8000) && equipment))
                    throw Error("Malformed rebuilt shop row or padding");
                if (word & 0x8000) {
                    equipment = true;
                    stock.equipment << quint16(word & 0x7fff);
                } else {
                    stock.consumables << word;
                }
            }
            if (!ended)
                throw Error("Unterminated rebuilt shop row");
            validate(id, stock);
            result.stock_[id] = stock;
        }
    }
    result.original_ = result.stock_;
    result.locations_.resize(result.size());
    const auto towns = world ? world->towns(rom) : LhaMember::find(rom, "ktenmain.bin").decode(rom);
    if (towns.size() % 28)
        throw Error("Truncated town archive");
    for (qsizetype at = 0; at < towns.size(); at += 28) {
        const auto shop = u8(towns, at + 27);
        if (!shop || u8(towns, at + 24) == 255)
            continue;
        if (shop >= result.size())
            throw Error("Town references an unknown shop");
        auto raw = towns.mid(at + 1, 20);
        if (raw.contains(char(0)))
            raw.truncate(raw.indexOf(char(0)));
        const auto label = QString::fromLatin1(raw);
        if (!result.locations_[shop].contains(label))
            result.locations_[shop].push_back(label);
    }
    return result;
}
const ShopStock &Shops::stock(int id) const {
    if (id < 0 || id >= stock_.size())
        throw Error("Unknown shop ID");
    return stock_[id];
}
const QStringList &Shops::locations(int id) const {
    stock(id);
    return locations_[id];
}
const ShopStock &Shops::originalStock(int id) const {
    stock(id);
    static const ShopStock empty;
    return id < original_.size() ? original_[id] : empty;
}
int Shops::addShop() {
    if (size() == 256)
        throw Error("All byte-sized shop IDs are in use");
    const int id = size();
    stock_.append(ShopStock{});
    locations_.append(QStringList{});
    return id;
}
void Shops::setStock(int id, const ShopStock &stock) {
    validate(id, stock);
    this->stock(id);
    stock_[id] = stock;
}
bool Shops::modified() const {
    return stock_ != original_;
}
int Shops::changedShops() const {
    int changed = 0;
    for (int id = 0; id < stock_.size(); ++id)
        if (id >= original_.size() || stock_[id] != original_[id])
            ++changed;
    return changed;
}
QJsonObject Shops::json() const {
    QJsonArray records;
    for (int id = 0; id < stock_.size(); ++id) {
        QJsonArray equipment, consumables;
        for (auto item : stock_[id].equipment)
            equipment << int(item);
        for (auto item : stock_[id].consumables)
            consumables << int(item);
        records << QJsonObject{{"id", id}, {"equipment", equipment}, {"consumables", consumables}};
    }
    return {{"version", stock_.size() == count ? 1 : 2}, {"records", records}};
}
void Shops::apply(const QJsonObject &object) {
    const int length = object["records"].toArray().size();
    if (!object["records"].isArray() || length < count || length > 256 ||
        !((object["version"] == 1 && length == count) || object["version"] == 2))
        throw Error("Shop project requires 36–256 ordered records");
    auto candidate = *this;
    const auto records = object["records"].toArray();
    candidate.stock_.resize(length);
    candidate.locations_.resize(length);
    auto ids = [](const QJsonValue &value) {
        if (!value.isArray())
            throw Error("Shop membership must be an array");
        QVector<quint16> result;
        for (const auto &id : value.toArray()) {
            const auto number = id.toDouble(-1);
            if (!id.isDouble() || !std::isfinite(number) || number != std::floor(number) ||
                number < 1 || number > 65535)
                throw Error("Invalid shop item ID");
            result.push_back(quint16(number));
        }
        return result;
    };
    for (int id = 0; id < length; ++id) {
        const auto row = records[id].toObject();
        if (row["id"] != id || row.size() != 3)
            throw Error("Malformed or unordered shop record");
        candidate.setStock(id, {ids(row["equipment"]), ids(row["consumables"])});
    }
    *this = std::move(candidate);
}
} // namespace lordly
