#include "engine/display_tool.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
namespace lordly {
namespace {
QByteArray canonical(const QJsonObject &site, QByteArray bytes) {
    if (site["name"] == "source-module" && bytes.size() >= 0x720)
        bytes.replace(0x520, 512, QByteArray(512, 0));
    return bytes;
}
bool matches(const QByteArray &bytes, const QJsonObject &site, const char *hash) {
    return sha256(bytes) == site[hash].toString().toLatin1();
}
} // namespace
QJsonObject DisplayTool::definition(ToolLabel kind) {
    QFile file(":/game/tools/displays.json");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing display tool source catalog");
    const auto features = QJsonDocument::fromJson(file.readAll()).object()["features"].toArray();
    if (features.size() != 2)
        throw Error("Invalid display tool source catalog");
    return features[kind == ToolLabel::Alignment ? 1 : 0].toObject();
}
DisplayTool DisplayTool::load(const Rom &rom, ToolLabel kind) {
    DisplayTool result;
    try {
        rom.requireSupported();
        if (rom.revision())
            throw Error("Additional value displays require US revision zero");
        bool retail = true, native = true, legacy = true;
        const auto description = definition(kind);
        const auto sites = description["sites"].toArray();
        if (sites.size() != 3)
            throw Error("Invalid display tool owner count");
        for (const auto &value : sites) {
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
            throw Error("The " + description["name"].toString() +
                        " display code or its allocation is unrecognized; existing bytes will be "
                        "preserved");
        result.enabled_ = result.original_ = !retail;
    } catch (const Error &e) {
        result.unavailable_ = QString::fromUtf8(e.what());
    }
    return result;
}
void DisplayTool::setEnabled(bool value) {
    if (!available())
        throw Error(unavailable_);
    enabled_ = value;
}
void DisplayTool::validateCompiled(const QJsonObject &site, const QByteArray &bytes, bool enabled) {
    if (bytes.size() != site["size"].toInt() ||
        !matches(enabled ? canonical(site, bytes) : bytes, site,
                 enabled ? "normalizedSha256" : "retailSha256"))
        throw Error("Compiled display owner differs from its maintained source: " +
                    site["source"].toString());
}
} // namespace lordly
