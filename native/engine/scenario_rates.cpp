#include "engine/scenario_rates.h"
#include "engine/custom_runtime.h"
#include "engine/promotion.h"
#include <cmath>
namespace lordly {
static quint32 integer(const QJsonValue &value) {
    const auto n = value.toDouble(-1);
    if (!value.isDouble() || !std::isfinite(n) || n != std::floor(n) || n < 0 || n > 4294967295.0)
        throw Error("Scenario rates require unsigned 32-bit whole numbers");
    return quint32(n);
}
static void validate(const ScenarioRoll &roll) {
    if (!roll.divisor || roll.passCount > roll.divisor)
        throw Error(
            "Scenario rate divisor must be positive and successful outcomes cannot exceed it");
}
const std::optional<ScenarioRoll> &ScenarioRates::branch(int key, bool normal) const {
    if (key < 0 || key >= count)
        throw Error("Scenario key must be a byte-sized ID");
    return rows_[key][normal ? 1 : 0];
}
void ScenarioRates::setBranch(int key, bool normal, const std::optional<ScenarioRoll> &value) {
    branch(key, normal);
    if (value)
        validate(*value);
    rows_[key][normal ? 1 : 0] = value;
}
static QJsonObject branchJson(const std::optional<ScenarioRoll> &value) {
    if (!value)
        return {{"mode", "inherit"}};
    return {{"mode", "override"},
            {"divisor", double(value->divisor)},
            {"passCount", double(value->passCount)}};
}
static std::optional<ScenarioRoll> parseBranch(const QJsonObject &object, bool legacy = false) {
    if (object.contains("mode") && !object["mode"].isString())
        throw Error("Scenario-rate mode must be a named mode");
    for (auto it = object.begin(); it != object.end(); ++it)
        if (it.key() != "mode" && it.key() != "divisor" && it.key() != "passCount" &&
            !(legacy && it.key() == "pass_count"))
            throw Error("Unknown scenario-rate field");
    const auto mode = object["mode"].toString(legacy ? "inherit" : QString());
    if (mode == "inherit")
        return std::nullopt;
    if (mode == "disabled")
        return ScenarioRoll{1, 0};
    if (mode != "override")
        throw Error("Unsupported scenario-rate mode");
    const ScenarioRoll value{integer(object["divisor"]),
                             integer(legacy && object.contains("pass_count")
                                         ? object["pass_count"]
                                         : object["passCount"])};
    validate(value);
    return value;
}
QJsonObject ScenarioRates::json() const {
    QJsonObject rows;
    for (int key = 0; key < count; ++key)
        if (rows_[key][0] || rows_[key][1])
            rows[QString::number(key)] = QJsonObject{{"alternate", branchJson(rows_[key][0])},
                                                     {"normal", branchJson(rows_[key][1])}};
    return {{"version", 1}, {"rows", rows}};
}
void ScenarioRates::apply(const QJsonObject &object) {
    if (object.size() != 2 || object["version"] != 1 || !object["rows"].isObject())
        throw Error("Invalid saved scenario rates");
    ScenarioRates next = *this;
    next.rows_ = {};
    const auto rows = object["rows"].toObject();
    for (auto it = rows.begin(); it != rows.end(); ++it) {
        bool ok = false;
        const auto key = it.key().toInt(&ok);
        if (!ok || QString::number(key) != it.key() || !it.value().isObject())
            throw Error("Invalid scenario-rate key");
        const auto row = it.value().toObject();
        if (row.size() != 2 || !row["normal"].isObject() || !row["alternate"].isObject())
            throw Error("Both scenario branches are required");
        next.setBranch(key, true, parseBranch(row["normal"].toObject()));
        next.setBranch(key, false, parseBranch(row["alternate"].toObject()));
    }
    rows_ = std::move(next.rows_);
}
void ScenarioRates::importLegacy(const QJsonObject &object) {
    auto next = *this;
    for (auto it = object.begin(); it != object.end(); ++it) {
        bool ok = false;
        const auto key = it.key().toInt(&ok);
        if (!ok || QString::number(key) != it.key() || !it.value().isObject())
            throw Error("Invalid browser scenario-rate key");
        const auto row = it.value().toObject();
        for (auto f = row.begin(); f != row.end(); ++f)
            if (f.key() != "slice" && f.key() != "normal" && f.key() != "alternate")
                throw Error("Unknown browser scenario-rate field");
        for (const auto &name : {"normal", "alternate"}) {
            if (row.contains(name) && !row[name].isObject() && !row[name].isNull())
                throw Error("Invalid browser scenario-rate branch");
            next.setBranch(key, QString(name) == "normal", parseBranch(row[name].toObject(), true));
        }
    }
    rows_ = std::move(next.rows_);
}
QByteArray ScenarioRates::bytes(const EncounterRate &global) const {
    QByteArray data(dataSize, 0);
    for (int key = 0; key < count; ++key) {
        int flags = 0;
        for (int normal = 0; normal < 2; ++normal) {
            const auto value = rows_[key][normal].value_or(ScenarioRoll{
                global.divisor(), normal ? global.normalPassCount() : global.alternatePassCount()});
            validate(value);
            put32(data, key * 16 + normal * 8, value.divisor);
            put32(data, key * 16 + normal * 8 + 4, value.passCount);
            if (rows_[key][normal])
                flags |= 1 << normal;
        }
        put8(data, 4096 + key, quint8(flags));
    }
    return data;
}
std::optional<ScenarioRatePlacement> ScenarioRates::placement(const Rom &rom) {
    const auto header = rom.bytes().mid(descriptorOffset, descriptorSize);
    if (!header.startsWith("LC05RATE"))
        return std::nullopt;
    rom.requireSupported();
    if (rom.revision() != 0 || header.size() != descriptorSize || u32(header, 8) != 1 ||
        u32(header, 16) != dataSize || u32(header, 20) != count || u32(header, 24) != rowSize ||
        header.mid(48, 12) != QByteArray(12, 0) || u32(header, 60) != crc32(header.first(60)))
        throw Error("Damaged scenario-rate descriptor");
    ScenarioRatePlacement result{u32(header, 12),
                                 {{"divisor", double(u32(header, 36))},
                                  {"normalPassCount", double(u32(header, 40))},
                                  {"alternatePassCount", double(u32(header, 44))}}};
    EncounterRate global;
    global.apply(result.global);
    const auto offset = result.offset;
    if (offset < 0x2800000 || offset % 16 || quint64(offset) + dataSize > 0x4000000 ||
        quint64(offset) + dataSize > quint64(rom.bytes().size()))
        throw Error("Scenario-rate allocation exceeds the cartridge range");
    auto code = rom.bytes().mid(EncounterRate::romOffset, EncounterRate::codeSize);
    if (const auto custom = CustomPlacement::read(rom))
        code = custom->canonicalReferences(code, EncounterRate::romOffset);
    const auto data = rom.bytes().mid(offset, dataSize);
    if (crc32(code) != u32(header, 32) || crc32(data) != u32(header, 28))
        throw Error("Scenario rates differ from their declared allocation");
    if (u32(code, 0x188) != (0x3c040000U | (offset >> 16)) ||
        u32(code, 0x18c) != (0x34840000U | (offset & 65535)))
        throw Error("Scenario selector references another rate allocation");
    put16(code, 0x18a, 0);
    put16(code, 0x18e, 0);
    const auto spec = EncounterRate::catalog();
    if (sha256(code) != spec["scenarioMaskedSha256"].toString().toLatin1())
        throw Error("Scenario dispatcher changed outside its rebuild");
    for (const auto &name : {"dma", "overlay"}) {
        const auto dependency = spec[name].toObject();
        if (sha256(rom.bytes().mid(dependency["start"].toInt(), dependency["size"].toInt())) !=
            dependency["sha256"].toString().toLatin1())
            throw Error(
                "Scenario rate runtime dependency changed outside its supported implementation");
    }
    for (int key = 0; key < count; ++key) {
        const auto flags = u8(data, 4096 + key);
        if (flags & ~3)
            throw Error("Unsupported scenario-rate flags");
        for (int normal = 0; normal < 2; ++normal) {
            const ScenarioRoll value{u32(data, key * 16 + normal * 8),
                                     u32(data, key * 16 + normal * 8 + 4)};
            validate(value);
            if (!(flags & (1 << normal)) &&
                value != ScenarioRoll{global.divisor(), normal ? global.normalPassCount()
                                                               : global.alternatePassCount()})
                throw Error("Inherited scenario rate disagrees with the global rate");
        }
    }
    return result;
}
ScenarioRates ScenarioRates::load(const Rom &rom) {
    ScenarioRates result;
    if (const auto prior = placement(rom))
        for (int key = 0; key < count; ++key) {
            const auto flags = u8(rom.bytes(), prior->offset + 4096 + key);
            for (int normal = 0; normal < 2; ++normal)
                if (flags & (1 << normal))
                    result.rows_[key][normal] =
                        ScenarioRoll{u32(rom.bytes(), prior->offset + key * 16 + normal * 8),
                                     u32(rom.bytes(), prior->offset + key * 16 + normal * 8 + 4)};
        }
    result.original_ = result.rows_;
    return result;
}
bool ScenarioRates::hasOverrides() const {
    for (const auto &row : rows_)
        if (row[0] || row[1])
            return true;
    return false;
}
int ScenarioRates::changedRows() const {
    int changed = 0;
    for (int key = 0; key < count; ++key)
        if (rows_[key] != original_[key])
            ++changed;
    return changed;
}
QByteArray ScenarioRates::definitions(quint32 offset) {
    return QString(".equ LC_NATIVE_GATE,1\n.equ LC_SCENARIO_RATES,1\n.equ LC_SCENARIO_HI,%1\n.equ "
                   "LC_SCENARIO_LO,%2\n")
        .arg(offset >> 16)
        .arg(offset & 65535)
        .toLatin1();
}
} // namespace lordly
