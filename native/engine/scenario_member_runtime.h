#pragma once
#include "engine/rom.h"
#include <QJsonObject>
namespace lordly {
QJsonObject scenarioMemberOwner();
bool scenarioMemberRuntimeInstalled(const Rom &rom);
}
