#pragma once
#include "engine/rom.h"
#include <QJsonObject>
namespace lordly {
QByteArray compileWorldEntries(const QByteArray &, const QJsonObject &);
QByteArray appendWorldUnlocks(const QByteArray &, const QJsonObject &);
} // namespace lordly
