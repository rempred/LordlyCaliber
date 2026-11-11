#include "engine/automatic_promotion.h"
#include "engine/automatic_promotion_owner.h"
#include <array>
#include <tuple>

namespace lordly {
namespace {
// func_001B6538 has no accepted C owner. These are its original unsigned
// comparisons; the alignment interval is (unsigned)(alignment - min) < span.
constexpr std::array<int, 5> angelSites{0x1b6680, 0x1b6698, 0x1b66b0, 0x1b66c8, 0x1b66cc};
constexpr std::array<quint32, 5> angelOpcodes{0x2c420000, 0x2c420000, 0x2c420000, 0x24420000, 0x2c420000};
int offset(int route, int field) {
    if (route >= 0 && route < 16 && field >= 0 && field < 8)
        return route * 11 + 3 + field;
    if (route >= 16 && route < 21 && field >= 0 && field < 2)
        return (route - 16) * 5 + 2 + field;
    return -1;
}
QByteArray decode(const QJsonObject &json, const char *key, const QByteArray &original) {
    const auto text = json[key].toString().toLatin1();
    const auto bytes = QByteArray::fromHex(text);
    if (!json[key].isString() || bytes.size() != original.size() || bytes.toHex() != text.toLower())
        throw Error("Invalid saved automatic class-change requirements");
    return bytes;
}
}
AutomaticPromotion AutomaticPromotion::load(const Rom &rom) {
    AutomaticPromotion p;
    if (rom.revision() != 0) return p;
    p.originalEvolution_ = p.evolution_ = rom.bytes().mid(evolutionOffset, 16 * 11);
    p.originalAttacks_ = p.attacks_ = rom.bytes().mid(attackOffset, 5 * 5);
    // Preserve unsupported code untouched and do not offer inert manual gates
    // in place of this route when its instruction contract is unavailable.
    bool known = true;
    std::array<int, 5> values{};
    for (int i = 0; i < int(angelSites.size()); ++i) {
        const auto word = u32(rom.bytes(), angelSites[i]);
        known &= (word & 0xffff0000U) == angelOpcodes[i];
        values[i] = word & 65535;
        p.angelInstructions_ += rom.bytes().mid(angelSites[i], 4);
    }
    const int minimum = -qint16(values[3]), maximum = minimum + values[4] - 1;
    known &= values[0] <= 255 && values[1] <= 255 && values[2] <= 255 &&
             minimum >= 0 && maximum >= minimum && maximum <= 255;
    if (known) {
        p.angel_ = QByteArray(8, 0);
        p.angel_[2] = char(values[0]); p.angel_[3] = char(values[1]); p.angel_[5] = char(values[2]);
        p.angel_[6] = char(minimum); p.angel_[7] = char(maximum);
        p.originalAngel_ = p.angel_;
    }
    return p;
}
QVector<AutomaticPromotion::Route> AutomaticPromotion::routes(int target) const {
    QVector<Route> result;
    for (int i = 0; i < evolution_.size() / 11; ++i)
        if (u8(evolution_, i * 11 + 1) == target)
            result.append({i, u8(evolution_, i * 11), target, "After-battle evolution"});
    const QStringList triggers{"Petrification", "Plasma damage", "Flame damage"};
    for (int i = 0; i < attacks_.size() / 5; ++i)
        if (u8(attacks_, i * 5 + 1) == target) {
            const int trigger = u8(attacks_, i * 5 + 4);
            result.append({16 + i, u8(attacks_, i * 5), target,
                           trigger < triggers.size() ? triggers[trigger]
                                                     : QString("Attack trigger %1").arg(trigger)});
        }
    if (available() && target == 0x24)
        result.append({angelRoute, -1, target, "After-mission transformation"});
    if (available() && (target == 0x2d || target == 0x2e))
        result.append({22, -1, target, "After-mission transformation"});
    return result;
}
bool AutomaticPromotion::supports(int route, int field) const {
    if (!available()) return false;
    if (route == angelRoute)
        return !angel_.isEmpty() && (field == 2 || field == 3 || field == 5 || field == 6 || field == 7);
    return offset(route, field) >= 0;
}
int AutomaticPromotion::value(int route, int field) const {
    if (!supports(route, field)) throw Error("This transformation does not check that stat");
    return route == angelRoute ? u8(angel_, field)
           : u8(route < 16 ? evolution_ : attacks_, offset(route, field));
}
void AutomaticPromotion::set(int route, int field, int v) {
    if (!supports(route, field) || v < 0 || v > 255)
        throw Error("Automatic class-change requirement is outside its supported range");
    if (route == angelRoute) {
        angel_[field] = char(v);
        if (field == 6 && v > u8(angel_, 7)) angel_[7] = char(v);
        if (field == 7 && v < u8(angel_, 6)) angel_[6] = char(v);
    } else {
        auto &table = route < 16 ? evolution_ : attacks_;
        table[offset(route, field)] = char(v);
    }
}
QJsonObject AutomaticPromotion::json() const {
    return {{"evolution", QString::fromLatin1(evolution_.toHex())},
            {"attacks", QString::fromLatin1(attacks_.toHex())},
            {"angel", QString::fromLatin1(angel_.toHex())}};
}
void AutomaticPromotion::apply(const QJsonObject &json) {
    if (json.isEmpty()) {
        evolution_ = originalEvolution_; attacks_ = originalAttacks_; angel_ = originalAngel_;
        return;
    }
    if (json.size() != 3) throw Error("Unknown automatic class-change settings");
    const auto evolution = decode(json, "evolution", originalEvolution_);
    const auto attacks = decode(json, "attacks", originalAttacks_);
    const auto angel = decode(json, "angel", originalAngel_);
    // This editor exposes requirements, not a replacement for the routing table.
    for (int i = 0; i < evolution.size(); i += 11)
        if (evolution.mid(i, 3) != originalEvolution_.mid(i, 3))
            throw Error("Automatic evolution source, target or element was changed");
    for (int i = 0; i < attacks.size(); i += 5)
        if (attacks.mid(i, 2) != originalAttacks_.mid(i, 2) || attacks[i + 4] != originalAttacks_[i + 4])
            throw Error("Attack transformation source, target or trigger was changed");
    if (!angel.isEmpty() && (angel[0] || angel[1] || angel[4] || u8(angel, 6) > u8(angel, 7)))
        throw Error("Invalid Angel Knight transformation requirements");
    evolution_ = evolution; attacks_ = attacks; angel_ = angel;
}
bool AutomaticPromotion::modified() const {
    return evolution_ != originalEvolution_ || attacks_ != originalAttacks_ || angel_ != originalAngel_;
}
QByteArray AutomaticPromotion::rebuild(const QByteArray &image) const {
    auto result = image;
    for (const auto &entry : {std::tuple{evolutionOffset, evolution_, originalEvolution_},
                              std::tuple{attackOffset, attacks_, originalAttacks_}}) {
        const auto &[at, data, original] = entry;
        if (data == original) continue;
        if (result.mid(at, original.size()) != original)
            throw Error("Automatic class-change table changed during export");
        result.replace(at, data.size(), data);
    }
    if (angel_ != originalAngel_) {
        for (int i = 0; i < int(angelSites.size()); ++i)
            if (result.mid(angelSites[i], 4) != angelInstructions_.mid(i * 4, 4))
                throw Error("Angel Knight threshold instruction changed during export");
        const std::array<int, 5> values{u8(angel_, 2), u8(angel_, 3), u8(angel_, 5),
                                       -u8(angel_, 6), u8(angel_, 7) - u8(angel_, 6) + 1};
        for (int i = 0; i < int(angelSites.size()); ++i)
            put32(result, angelSites[i], angelOpcodes[i] | quint16(values[i]));
    }
    return result;
}
QByteArray canonicalAutomaticRequirementOwner(const QByteArray &bytes, qsizetype romStart) {
    if (romStart != 0x1b6538 || bytes.size() != 0xedc) return bytes;
    std::array<int, 5> values{};
    for (int i = 0; i < int(angelSites.size()); ++i) {
        const auto word = u32(bytes, angelSites[i] - romStart);
        if ((word & 0xffff0000U) != angelOpcodes[i]) return bytes;
        values[i] = word & 65535;
    }
    const int minimum = -qint16(values[3]), maximum = minimum + values[4] - 1;
    if (values[0] > 255 || values[1] > 255 || values[2] > 255 ||
        minimum < 0 || maximum < minimum || maximum > 255) return bytes;
    auto canonical = bytes;
    constexpr std::array<int, 5> retail{60, 61, 57, 65536 - 50, 51};
    for (int i = 0; i < int(angelSites.size()); ++i)
        put32(canonical, angelSites[i] - romStart, angelOpcodes[i] | retail[i]);
    return canonical;
}
} // namespace lordly
