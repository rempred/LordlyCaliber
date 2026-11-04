#pragma once
#include "engine/project.h"
#include <optional>
class QWidget;
namespace lordly {
std::optional<QJsonObject> editEventTree(Project *,int row,QJsonObject,QWidget *,QJsonObject *flagState=nullptr);
std::optional<QJsonObject> editEventTrigger(Project *,int row,QJsonObject,QWidget *);
QString eventTreeSummary(const QJsonObject &,int row,const SceneEvents *flags=nullptr);
QString eventConditionSummary(const QJsonObject &,int row,const SceneEvents *flags=nullptr);
QString eventTargetSummary(int row,int group,int target);
QMap<int,QString> eventTargets(Project *,int row,int group);
}
