#include "engine/high_attack.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
namespace lordly {
namespace {
QByteArray source(const QString &name) {
    QFile file(":/game/tools/" + name);
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing High Attack source: " + name);
    return file.readAll();
}
bool matches(const QByteArray &bytes, const QJsonObject &site, const char *key) {
    return bytes.size() == site["size"].toInt() && sha256(bytes) == site[key].toString().toLatin1();
}
} // namespace
QJsonObject HighAttack::definition() {
    const auto result = QJsonDocument::fromJson(source("high-attack.json")).object();
    if (result["version"] != 23 || result["sites"].toArray().size() != 11)
        throw Error("Invalid High Attack source catalog");
    return result;
}
HighAttack HighAttack::load(const Rom &rom) {
    HighAttack result;
    try {
        rom.requireSupported();
        if (rom.revision())
            throw Error("High Attack requires US revision zero");
        bool retail = true, native = true, legacy = true;
        for (const auto &value : definition()["sites"].toArray()) {
            const auto site = value.toObject();
            const auto bytes = rom.bytes().mid(site["rom"].toInt(), site["size"].toInt());
            retail &= matches(bytes, site, "retailSha256");
            native &= matches(bytes, site, "nativeSha256");
            legacy &= matches(bytes, site, "legacySha256");
        }
        if (retail)
            result.installation_ = Installation::Retail;
        else if (native)
            result.installation_ = Installation::Native;
        else if (legacy)
            result.installation_ = Installation::Legacy;
        else
            throw Error("High Attack code or its allocation is unrecognized; existing bytes "
                        "will be preserved");
        result.enabled_ = result.original_ = !retail;
    } catch (const Error &e) {
        result.unavailable_ = QString::fromUtf8(e.what());
    }
    return result;
}
void HighAttack::setEnabled(bool value) {
    if (!available())
        throw Error(unavailable_);
    enabled_ = value;
}
QByteArray HighAttack::assembly(const QJsonObject &site, bool enabled) {
    if (site.contains("hookIndex")) {
        QByteArray flags;
        for (int i = 0; i < 8; ++i)
            flags += QString(".set HA_SITE_%1,%2\n")
                         .arg(i)
                         .arg(i == site["hookIndex"].toInt() ? 1 : 0)
                         .toLatin1();
        return flags + QString(".set HA_RETAIL,%1\n").arg(enabled ? 0 : 1).toLatin1() +
               source(site["source"].toString());
    }
    if (!enabled)
        return QString(".space %1,%2\n")
            .arg(site["size"].toInt())
            .arg(site["name"] == "module" ? 255 : 0)
            .toLatin1();
    return source(site["source"].toString());
}
void HighAttack::validateCompiled(const QJsonObject &site, const QByteArray &bytes, bool enabled) {
    if (!matches(bytes, site, enabled ? "nativeSha256" : "retailSha256"))
        throw Error("Compiled High Attack owner differs from maintained source: " +
                    site["name"].toString());
}
} // namespace lordly
