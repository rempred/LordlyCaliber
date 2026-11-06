#pragma once
#include "engine/project.h"
#include <optional>
class QWidget;
namespace lordly {
std::optional<QJsonObject> editConsumableVisual(QWidget *, const Project &, int item);
}
