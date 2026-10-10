#include "engine/scenario_member_runtime.h"
#include "engine/class_data.h"
#include <QFile>
#include <QJsonDocument>
namespace lordly {
QJsonObject scenarioMemberOwner() {
    QFile file(":/game/scenarios/squad-member.json");
    if (!file.open(QIODevice::ReadOnly)) throw Error("Missing scenario member owner catalog");
    return QJsonDocument::fromJson(file.readAll()).object();
}
bool scenarioMemberRuntimeInstalled(const Rom &rom) {
    const auto owner = scenarioMemberOwner();
    const int at = owner["start"].toInt();
    auto code = rom.bytes().mid(at, owner["size"].toInt());
    if (const auto classes = ClassPlacement::read(rom)) code = classes->canonicalReferences(code, at);
    return sha256(code) == owner["cSourceSha256"].toString().toLatin1();
}
}
