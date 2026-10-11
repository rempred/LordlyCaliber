#include "engine/encounter_rate.h"
#include "engine/custom_runtime.h"
#include "engine/scenario_rates.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <cmath>
namespace lordly {
QJsonObject EncounterRate::catalog() {
    QFile file(":/game/encounters/dispatcher.json");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing encounter source catalog");
    return QJsonDocument::fromJson(file.readAll()).object();
}
QMap<QString, quint32> EncounterRate::serviceAddresses() {
    QMap<QString, quint32> result;
    const auto calls = catalog()["calls"].toObject();
    for (auto it = calls.begin(); it != calls.end(); ++it)
        result[it.key()] = quint32(it.value().toDouble());
    return result;
}
static quint32 integer(const QJsonValue &v, double minimum = 0) {
    const auto n = v.toDouble(-1);
    if (!v.isDouble() || !std::isfinite(n) || n != std::floor(n) || n < minimum || n > 4294967295.0)
        throw Error("Encounter rate requires whole numbers within the unsigned 32-bit range");
    return quint32(n);
}
QJsonObject EncounterRate::json() const {
    return {{"divisor", double(divisor_)},
            {"normalPassCount", double(normal_)},
            {"alternatePassCount", double(alternate_)}};
}
double EncounterRate::passingFraction(quint32 divisor, quint32 passCount) {
    if (!divisor || passCount > divisor)
        throw Error("Invalid encounter outcome ratio");
    constexpr quint64 range = quint64(1) << 30;
    const quint64 passing =
        (range / divisor) * passCount + qMin(range % divisor, quint64(passCount));
    return double(passing) / double(range);
}
void EncounterRate::apply(const QJsonObject &object) {
    if (object.size() != 3)
        throw Error("Encounter rate requires a divisor and both pass counts");
    const auto divisor = integer(object["divisor"], 1), normal = integer(object["normalPassCount"]),
               alternate = integer(object["alternatePassCount"]);
    if (normal > divisor || alternate > divisor)
        throw Error("Successful outcomes cannot exceed the encounter divisor");
    divisor_ = divisor;
    normal_ = normal;
    alternate_ = alternate;
}
EncounterRate EncounterRate::load(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision() != 0)
        throw Error("Native encounter rates currently support US Rev0");
    const auto spec = catalog();
    const auto random = spec["random"].toObject();
    if (sha256(rom.bytes().mid(random["start"].toInt(), random["size"].toInt())) !=
        random["sha256"].toString().toLatin1())
        throw Error("Encounter random-number generator differs from the supported implementation");
    if (const auto scenario = ScenarioRates::placement(rom)) {
        EncounterRate result;
        result.apply(scenario->global);
        result.original_ = result.json();
        return result;
    }
    auto source = rom.bytes().mid(romOffset, codeSize);
    if (const auto custom = CustomPlacement::read(rom))
        source = custom->canonicalReferences(source, romOffset);
    auto masked = source;
    QJsonObject values{{"divisor", 0}, {"normalPassCount", 0}, {"alternatePassCount", 0}};
    for (const auto &entry : spec["fields"].toArray()) {
        const auto field = entry.toObject();
        const auto at = field["offset"].toInt(), shift = field["half"] == "high" ? 16 : 0;
        const auto key = field["key"].toString();
        values[key] =
            double(quint32(values[key].toDouble()) | (quint32(u16(source, at + 2)) << shift));
        put16(masked, at + 2, 0);
    }
    EncounterRate result;
    if (sha256(masked) == spec["nativeMaskedSha256"].toString().toLatin1()) {
        result.apply(values);
    } else {
        auto canonical = source;
        const QList<QPair<int, quint32>> fields = {
            {0x188, 0x3c110001}, {0x18c, 0x36311940}, {0x19c, 0x24100032}, {0x1a0, 0x2410000a}};
        for (const auto [at, word] : fields) {
            if ((u32(source, at) & 0xffff0000U) != (word & 0xffff0000U))
                throw Error("Unrecognized encounter rate instruction");
            put32(canonical, at, word);
        }
        const auto branch = u32(source, 0x1c8);
        if (branch != 0x14600090 && branch != 0x10000090 && branch != 0)
            throw Error("Unrecognized encounter gate instruction");
        put32(canonical, 0x1c8, 0x14600090);
        if (sha256(canonical) != spec["retailSha256"].toString().toLatin1())
            throw Error("Encounter selection code has an unsupported runtime modification");
        const quint32 divisor = (quint32(u16(source, 0x18a)) << 16) | u16(source, 0x18e);
        auto count = [&](int at) -> quint32 {
            if (!branch)
                return divisor;
            if (branch == 0x10000090)
                return 0;
            return quint32(
                qMin(quint64(divisor), quint64(quint32(qint16(u16(source, at + 2)))) + 1));
        };
        result.apply({{"divisor", double(divisor)},
                      {"normalPassCount", double(count(0x19c))},
                      {"alternatePassCount", double(count(0x1a0))}});
    }
    result.original_ = result.json();
    return result;
}
QByteArray EncounterRate::assemblyDefinitions(bool native) const {
    QByteArray result =
        QByteArray(".equ LC_SCENARIO_RATES,0\n.equ LC_NATIVE_GATE,") + (native ? "1\n" : "0\n");
    for (const auto &pair : QList<QPair<QString, quint32>>{
             {"DIVISOR", divisor_}, {"NORMAL", normal_}, {"ALTERNATE", alternate_}}) {
        result += QString(".equ LC_%1_HI, %2\n.equ LC_%1_LO, %3\n")
                      .arg(pair.first)
                      .arg(pair.second >> 16)
                      .arg(pair.second & 65535)
                      .toLatin1();
    }
    return result;
}
void EncounterRate::importLegacy(const QJsonObject &object) {
    const QStringList ratioKeys = {"pass_count", "micro_basis_points", "basis_points", "percent",
                                   "multiplier"};
    const QStringList rootKeys =
        ratioKeys + QStringList{"schema", "divisor", "linked", "normal", "alternate"};
    for (auto it = object.begin(); it != object.end(); ++it)
        if (!rootKeys.contains(it.key()))
            throw Error("Unknown imported global encounter-rate field");
    const auto divisor = object.contains("divisor") ? integer(object["divisor"], 1) : divisor_;
    if (object.contains("schema") && object["schema"] != 2)
        throw Error("Unsupported browser encounter-rate schema");
    if (object.contains("linked") && !object["linked"].isBool())
        throw Error("Invalid linked encounter rate");
    auto branch = [&](const QJsonObject &entry, quint32 fallback, int retailPass) -> quint32 {
        for (auto it = entry.begin(); it != entry.end(); ++it)
            if (!(object.contains("normal") || object.contains("alternate") ? ratioKeys : rootKeys)
                     .contains(it.key()))
                throw Error("Unknown imported encounter-rate branch field");
        if (entry.contains("pass_count"))
            return qMin(divisor, integer(entry["pass_count"]));
        for (const auto &key : {"micro_basis_points", "basis_points", "percent", "multiplier"}) {
            if (!entry.contains(key))
                continue;
            const auto value = entry[key].toDouble(-1);
            if (!entry[key].isDouble() || !std::isfinite(value) || value < 0)
                throw Error("Invalid imported encounter ratio");
            double passes = key == QString("multiplier")
                                ? value * retailPass
                                : value * divisor /
                                      (key == QString("micro_basis_points") ? 1000000.0
                                       : key == QString("basis_points")     ? 10000.0
                                                                            : 100.0);
            if (!std::isfinite(passes))
                throw Error("Imported encounter ratio is too large");
            return quint32(qBound(0.0, std::round(passes), double(divisor)));
        }
        if (entry.isEmpty())
            return quint32(
                qBound(0.0, std::round(double(fallback) * divisor / divisor_), double(divisor)));
        throw Error("Imported encounter branch has no supported ratio");
    };
    quint32 normal, alternate;
    if (object.contains("normal") || object.contains("alternate")) {
        for (const auto &key : {"normal", "alternate"})
            if (object.contains(key) && !object[key].isObject())
                throw Error("Invalid imported encounter branch");
        normal = branch(object["normal"].toObject(), normal_, 51);
        alternate = object["linked"].toBool()
                        ? normal
                        : branch(object["alternate"].toObject(), alternate_, 11);
    } else {
        normal = alternate = branch(object, normal_, 51);
    }
    apply({{"divisor", double(divisor)},
           {"normalPassCount", double(normal)},
           {"alternatePassCount", double(alternate)}});
}
} // namespace lordly
