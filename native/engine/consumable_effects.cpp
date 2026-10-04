#include "engine/consumable_effects.h"
#include <QFile>
#include <QJsonDocument>
#include <QSet>
#include <cmath>
namespace lordly {
QJsonObject ConsumableEffects::catalog() {
    QFile file(":/game/consumables/effects.json");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing consumable effect source catalog");
    return QJsonDocument::fromJson(file.readAll()).object();
}
QJsonArray ConsumableEffects::definitions() {
    return catalog()["models"].toArray();
}
QMap<QString, quint32> ConsumableEffects::serviceAddresses() {
    QMap<QString, quint32> result;
    const auto calls = catalog()["calls"].toObject();
    for (auto it = calls.begin(); it != calls.end(); ++it)
        result[it.key()] = quint32(it.value().toDouble());
    return result;
}
QJsonObject ConsumableEffects::validate(const QJsonObject &definition, const QJsonObject &values) {
    auto number = [&](const QString &key) {
        const auto v = values[key];
        const auto n = v.toDouble(-1000000);
        if (!v.isDouble() || !std::isfinite(n) || n != std::floor(n) ||
            n < definition["domainMin"].toInt() || n > definition["domainMax"].toInt())
            throw Error("Consumable effect value is outside its supported range");
        return int(n);
    };
    if (definition["kind"] == "magnitude") {
        if (values.size() != 1)
            throw Error("This consumable effect requires one magnitude");
        return {{"magnitude", number("magnitude")}};
    }
    if (values.size() != 2)
        throw Error("This consumable effect requires a minimum and maximum");
    const auto minimum = number("minimum"), maximum = number("maximum");
    if (minimum > maximum)
        throw Error("Effect minimum cannot exceed its maximum");
    return {{"minimum", minimum}, {"maximum", maximum}};
}
ConsumableEffects ConsumableEffects::load(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision() != 0)
        throw Error("Native consumable effects currently support US Rev0");
    auto canonical = rom.bytes().mid(romOffset, codeSize);
    if (canonical.size() != codeSize)
        throw Error("Truncated consumable effect routine");
    ConsumableEffects result;
    for (const auto &value : definitions()) {
        const auto def = value.toObject();
        QJsonObject fields;
        for (const auto &entry : def["fields"].toArray()) {
            const auto field = entry.toObject();
            const auto at = field["offset"].toInt();
            const auto retail = quint32(field["retail"].toDouble()), word = u32(canonical, at);
            if ((word & 0xffff0000U) != (retail & 0xffff0000U))
                throw Error("Unrecognized consumable effect instruction");
            const auto kind = field["kind"].toString();
            const auto parameter =
                kind == "minimum" ? int(qint16(word & 65535)) : int(word & 65535);
            if (fields.contains(kind) && fields[kind] != parameter)
                throw Error("Consumable effect paths have conflicting magnitudes");
            fields[kind] = parameter;
            put32(canonical, at, retail);
        }
        if (def["kind"] == "range") {
            fields["maximum"] = fields["minimum"].toInt() + fields["width"].toInt() - 1;
            fields.remove("width");
        }
        result.models_[def["key"].toString()] = validate(def, fields);
    }
    const auto spec = catalog(), dispatch = spec["dispatch"].toObject();
    if (sha256(canonical) != spec["retailSha256"].toString().toLatin1() ||
        sha256(rom.bytes().mid(dispatch["start"].toInt(), dispatch["size"].toInt())) !=
            dispatch["retailSha256"].toString().toLatin1())
        throw Error("Consumable effects or their dispatch table changed outside the supported "
                    "source model");
    result.original_ = result.models_;
    return result;
}
void ConsumableEffects::setModel(const QString &key, const QJsonObject &values) {
    for (const auto &value : definitions()) {
        const auto def = value.toObject();
        if (def["key"] == key) {
            models_[key] = validate(def, values);
            return;
        }
    }
    throw Error("Unknown consumable effect model");
}
void ConsumableEffects::apply(const QJsonObject &object) {
    if (object["version"] != 1 || !object["models"].isObject())
        throw Error("Unsupported consumable effect project format");
    const auto values = object["models"].toObject();
    if (values.size() != definitions().size())
        throw Error("Consumable effect project must contain all supported models");
    auto next = *this;
    for (auto it = values.begin(); it != values.end(); ++it) {
        if (!it.value().isObject())
            throw Error("Invalid consumable effect model");
        next.setModel(it.key(), it.value().toObject());
    }
    *this = std::move(next);
}
void ConsumableEffects::importLegacy(const QJsonObject &object, int version) {
    auto next = *this;
    for (auto it = object.begin(); it != object.end(); ++it) {
        bool found = false;
        for (const auto &value : definitions()) {
            const auto def = value.toObject();
            if (def["projectKey"] != it.key())
                continue;
            if (!it.value().isObject())
                throw Error("Invalid imported effect");
            const auto entry = it.value().toObject();
            if (def["kind"] == "magnitude") {
                if (version < 13)
                    throw Error("Magnitude edits require browser project version13 or later");
                next.setModel(def["key"].toString(), entry);
            } else {
                if (version < 12 || entry.size() != 2 || !entry.contains("deltaMin") ||
                    !entry.contains("deltaMax"))
                    throw Error("Invalid imported effect range");
                next.setModel(def["key"].toString(),
                              {{"minimum", entry["deltaMin"]}, {"maximum", entry["deltaMax"]}});
            }
            found = true;
            break;
        }
        if (!found)
            throw Error("Unsupported imported effect; stat boosters11-16 share one entry");
    }
    *this = std::move(next);
}
static int parameter(const QJsonObject &model, const QString &kind) {
    return kind == "width" ? model["maximum"].toInt() - model["minimum"].toInt() + 1
                           : model[kind].toInt();
}
QByteArray ConsumableEffects::assemblyDefinitions() const {
    QByteArray result;
    QSet<QString> emitted;
    for (const auto &value : definitions()) {
        const auto def = value.toObject(),
                   model = validate(def, models_[def["key"].toString()].toObject());
        for (const auto &entry : def["fields"].toArray()) {
            const auto field = entry.toObject();
            const auto symbol = field["symbol"].toString();
            if (emitted.contains(symbol))
                continue;
            emitted.insert(symbol);
            result += QString(".equ %1, %2\n")
                          .arg(symbol)
                          .arg(qint16(parameter(model, field["kind"].toString()) & 65535))
                          .toLatin1();
        }
    }
    return result;
}
QByteArray ConsumableEffects::expectedCode(const QByteArray &source) const {
    if (source.size() != codeSize)
        throw Error("Consumable source owner extent changed");
    auto result = source;
    for (const auto &value : definitions()) {
        const auto def = value.toObject(),
                   model = validate(def, models_[def["key"].toString()].toObject());
        for (const auto &entry : def["fields"].toArray()) {
            const auto field = entry.toObject();
            const auto at = field["offset"].toInt();
            put32(result, at,
                  (quint32(field["retail"].toDouble()) & 0xffff0000U) |
                      quint16(parameter(model, field["kind"].toString())));
        }
    }
    return result;
}
} // namespace lordly
