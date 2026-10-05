#include "engine/tool_settings.h"
#include <QFile>
#include <QJsonDocument>
#include <bit>
#include <cmath>
namespace lordly {
namespace {
quint32 scaleBits(int percent) {
    return std::bit_cast<quint32>(float(double(percent) / 100.));
}
QByteArray canonical(QByteArray bytes, int part) {
    if (!part) {
        put16(bytes, 2, 0x3f80);
        put16(bytes, 6, 0);
    }
    return bytes;
}
bool matches(const QByteArray &bytes, const QJsonObject &site, const char *hash) {
    return bytes.size() == site["size"].toInt() &&
           sha256(bytes) == site[hash].toString().toLatin1();
}
} // namespace
QJsonArray ToolSettings::experienceSites() {
    QFile file(":/game/tools/experience.json");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing experience source catalog");
    const auto sites = QJsonDocument::fromJson(file.readAll()).object()["sites"].toArray();
    if (sites.size() != 3)
        throw Error("Invalid experience source catalog");
    return sites;
}
ToolSettings ToolSettings::load(const Rom &rom) {
    ToolSettings result;
    result.chaos_ = ChaosCounter::load(rom);
    result.luck_ = DisplayTool::load(rom, ToolLabel::Luck);
    result.alignment_ = DisplayTool::load(rom, ToolLabel::Alignment);
    result.highAttack_ = HighAttack::load(rom);
    try {
        rom.requireSupported();
        if (rom.revision())
            throw Error("Experience editing currently requires US revision zero");
        const auto sites = experienceSites();
        bool retail = true, patched = true;
        for (int i = 0; i < sites.size(); ++i) {
            const auto site = sites[i].toObject();
            const auto bytes = rom.bytes().mid(site["rom"].toInt(), site["size"].toInt());
            retail &= matches(bytes, site, "retailSha256");
            patched &= matches(canonical(bytes, i), site, "patchedSha256");
        }
        if (!retail) {
            if (!patched)
                throw Error(
                    "Experience code is unrecognized; its existing bytes will be preserved");
            const auto at = sites[0].toObject()["rom"].toInt();
            const auto bits = quint32(u16(rom.bytes(), at + 2)) << 16 | u16(rom.bytes(), at + 6);
            const auto value = std::bit_cast<float>(bits);
            if (!std::isfinite(value) || value < 0 || value > 3)
                throw Error("Experience multiplier is outside the supported range");
            const int percent = int(std::round(double(value) * 100));
            if (scaleBits(percent) != bits)
                throw Error("Experience multiplier does not use an integer percentage");
            result.percent_ = result.original_ = percent;
        }
    } catch (const Error &e) {
        result.unavailable_ = QString::fromUtf8(e.what());
    }
    return result;
}
void ToolSettings::setExperiencePercent(int value) {
    if (!available())
        throw Error(unavailable_);
    if (value < 0 || value > 300)
        throw Error("Experience scale must be between 0% and 300%");
    percent_ = value;
}
void ToolSettings::apply(const QJsonObject &object) {
    const auto value = object["experienceScalePercent"];
    const auto number = value.toDouble(-1);
    const bool old = object["version"] == 1 && object.size() == 2;
    const bool counter =
        object["version"] == 2 && object.size() == 3 && object["chaosFrameCounter"].isBool();
    const bool current =
        object["version"] == 4 && object.size() == 6 && object["highAttackStreamSplit"].isBool();
    const bool displays = ((object["version"] == 3 && object.size() == 5) || current) &&
                          object["chaosFrameCounter"].isBool() &&
                          object["characterCardLuck"].isBool() &&
                          object["squadMenuAlignment"].isBool();
    if ((!old && !counter && !displays) || !value.isDouble() || !std::isfinite(number) ||
        number != std::floor(number) || number < 0 || number > 300)
        throw Error("Invalid Tools project settings");
    auto candidate = *this;
    if (candidate.percent_ != int(number))
        candidate.setExperiencePercent(int(number));
    if ((counter || displays) && object["chaosFrameCounter"].toBool() != candidate.chaos_.enabled())
        candidate.chaos_.setEnabled(object["chaosFrameCounter"].toBool());
    if (displays && object["characterCardLuck"].toBool() != candidate.luck_.enabled())
        candidate.luck_.setEnabled(object["characterCardLuck"].toBool());
    if (displays && object["squadMenuAlignment"].toBool() != candidate.alignment_.enabled())
        candidate.alignment_.setEnabled(object["squadMenuAlignment"].toBool());
    if (current && object["highAttackStreamSplit"].toBool() != candidate.highAttack_.enabled())
        candidate.highAttack_.setEnabled(object["highAttackStreamSplit"].toBool());
    *this = candidate;
}
QByteArray ToolSettings::assemblyDefinitions(int part) const {
    if (!available() || part < 0 || part > 2)
        throw Error("Unavailable experience source region");
    const auto bits = scaleBits(percent_);
    return QString(".set XP_SETUP,%1\n.set XP_MINIMUM,%2\n.set XP_RETAIL,%3\n.set XP_HI,%4\n.set "
                   "XP_LO,%5\n")
        .arg(part == 0 ? 1 : 0)
        .arg(part == 1 ? 1 : 0)
        .arg(percent_ == 100 ? 1 : 0)
        .arg(bits >> 16)
        .arg(bits & 65535)
        .toLatin1();
}
void ToolSettings::validateCompiled(int part, const QByteArray &bytes) const {
    const auto sites = experienceSites();
    if (part < 0 || part >= sites.size())
        throw Error("Unknown experience source region");
    const auto site = sites[part].toObject();
    if (!matches(percent_ == 100 ? bytes : canonical(bytes, part), site,
                 percent_ == 100 ? "retailSha256" : "patchedSha256"))
        throw Error("Compiled experience code differs outside its authored parameter");
    if (!part && percent_ != 100 &&
        ((quint32(u16(bytes, 2)) << 16) | u16(bytes, 6)) != scaleBits(percent_))
        throw Error("Compiled experience multiplier differs from the selected value");
}
} // namespace lordly
