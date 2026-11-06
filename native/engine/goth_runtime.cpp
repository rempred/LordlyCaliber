#include "engine/goth_runtime.h"
#include "engine/mission_runtime.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
namespace lordly {
QJsonObject GothRuntime::definition() {
    QFile file(":/game/tools/goth-owner.json");
    if (!file.open(QIODevice::ReadOnly)) throw Error("Missing C settlement catalog");
    return QJsonDocument::fromJson(file.readAll()).object();
}
std::optional<GothRuntime> GothRuntime::read(const Rom &rom) {
    const auto header = rom.bytes().mid(headerOffset, 16);
    if (!header.startsWith("LCGOTH01")) return std::nullopt;
    if (header.size() != 16 || u16(header, 8) < 1 || u16(header, 8) > 1000 ||
        u16(header, 10) < 1 || u16(header, 10) > 1000 || u32(header, 12) > 1)
        throw Error("Invalid C settlement settings");
    GothRuntime result{u16(header, 8), u16(header, 10), u32(header, 12) != 0};
    const auto owner = definition()[result.reserve ? "reserve" : "plain"].toObject();
    if (sha256(canonicalMissionImage(rom).mid(offset, size - 16)) != owner["sha256"].toString().toLatin1())
        throw Error("Settlement C changed outside its declared settings");
    for (const auto &v : owner["tables"].toArray()) {
        const auto table = v.toObject();
        if (sha256(rom.bytes().mid(table["start"].toInt(), table["size"].toInt())) !=
            table["sha256"].toString().toLatin1())
            throw Error("Settlement switch table changed outside its rebuild");
    }
    return result;
}
}
