#pragma once
#include <QJsonArray>
class QWidget;
namespace lordly {
class Project;
QString fixedConsumableDescription(const QJsonArray &steps);
bool assignConsumableUse(QWidget *parent, Project &draft, int item);
} // namespace lordly
