#pragma once
#include "engine/project.h"
#include <QJsonArray>
#include <functional>
#include <optional>
class QWidget;
class QTreeWidget;
class QTreeWidgetItem;
namespace lordly {
constexpr int ConsumableNodeRole = Qt::UserRole + 1;
constexpr int ConsumableBranchRole = Qt::UserRole + 2;
using MoveConsumableEffect = std::function<bool(QTreeWidgetItem *, QTreeWidgetItem *)>;
QTreeWidget *createConsumableTreeView(MoveConsumableEffect moved);
void moveConsumableRow(QTreeWidgetItem *effect, QTreeWidgetItem *branch);
QJsonArray consumableSteps(const QTreeWidgetItem *branch);
QString consumableStepLabel(const Project &project, const QJsonObject &node);
std::optional<QJsonArray> editConsumableTree(QWidget *, const Project &, int,
                                             const QList<int> &path = {});
} // namespace lordly
