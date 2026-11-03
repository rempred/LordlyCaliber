#pragma once
#include "engine/rom.h"
#include <QJsonObject>
namespace lordly {
// Reusable holes retain every other game ID. This directory is editor metadata;
// the game continues to consume the ordinary class/item/attack/shop tables.
QJsonObject readDeletedContent(const Rom &);
QByteArray writeDeletedContent(QByteArray, const QJsonObject &);
void validateDeletedContent(const QJsonObject &);
}
