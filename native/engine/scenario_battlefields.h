#pragma once
#include "engine/rom.h"
#include <QJsonArray>

namespace lordly {
// Project snapshots remain available when an exported ROM is reopened alone.
// The game consumes the ordinary resource directories, never this metadata.
QJsonArray readScenarioBattlefields(const Rom &);
QByteArray writeScenarioBattlefields(const Rom &, const QJsonArray &);
void validateScenarioBattlefieldMetadata(const QJsonArray &);
} // namespace lordly
