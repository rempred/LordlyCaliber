#include "engine/chaos_counter.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
namespace lordly {
namespace {
QByteArray canonical(const QJsonObject &site, QByteArray bytes) {
    if (site["name"] == "source-module" && bytes.size() >= 0x880)
        bytes.replace(0x700, 384, QByteArray(384, 0));
    return bytes;
}
bool matches(const QByteArray &bytes, const QJsonObject &site, const char *hash) {
    return sha256(bytes) == site[hash].toString().toLatin1();
}
} // namespace
QJsonArray ChaosCounter::sites() {
    QFile file(":/game/tools/chaos.json");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing Chaos Frame source catalog");
    const auto sites = QJsonDocument::fromJson(file.readAll()).object()["sites"].toArray();
    if (sites.size() != 4)
        throw Error("Invalid Chaos Frame source catalog");
    return sites;
}
ChaosCounter ChaosCounter::load(const Rom &rom) {
    ChaosCounter result;
    try {
        rom.requireSupported();
        if (rom.revision())
            throw Error("Chaos Frame counter requires US revision zero");
        bool retail = true, native = true, legacy = true;
        for (const auto &value : sites()) {
            const auto site = value.toObject();
            const auto bytes = rom.bytes().mid(site["rom"].toInt(), site["size"].toInt());
            retail &= matches(bytes, site, "retailSha256");
            native &= matches(canonical(site, bytes), site, "normalizedSha256");
            legacy &=
                matches(rom.bytes().mid(site["legacyRom"].toInt(), site["legacyBytes"].toInt()),
                        site, "legacySha256");
        }
        if (retail)
            result.installation_ = Installation::Retail;
        else if (native)
            result.installation_ = Installation::Native;
        else if (legacy)
            result.installation_ = Installation::Legacy;
        else
            throw Error("Chaos Frame code or its allocation is unrecognized; existing bytes will "
                        "be preserved");
        result.enabled_ = result.original_ = !retail;
    } catch (const Error &e) {
        result.unavailable_ = QString::fromUtf8(e.what());
    }
    return result;
}
void ChaosCounter::setEnabled(bool value) {
    if (!available())
        throw Error(unavailable_);
    enabled_ = value;
}
void ChaosCounter::validateCompiled(const QJsonObject &site, const QByteArray &bytes,
                                    bool enabled) {
    if (bytes.size() != site["size"].toInt() ||
        !matches(enabled ? canonical(site, bytes) : bytes, site,
                 enabled ? "normalizedSha256" : "retailSha256"))
        throw Error("Compiled Chaos Frame owner differs from its maintained source: " +
                    site["name"].toString());
}
} // namespace lordly
