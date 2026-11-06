#pragma once
#include "engine/project.h"
#include <optional>
class QWidget;
namespace lordly {
enum class ChartTreeEdit { Edit, AddDecision, AddAction, Remove };
std::optional<QJsonObject> editEventTree(Project *,int row,QJsonObject,QWidget *,QJsonObject *flagState=nullptr,const QVector<int> &path={},ChartTreeEdit edit=ChartTreeEdit::Edit,bool fromChart=false);
std::optional<QJsonObject> editEventTrigger(Project *,int row,QJsonObject,QWidget *);
std::optional<QJsonObject> editRetailCondition(Project *,int row,int group,QJsonObject,QWidget *,QJsonObject *flags);
QString eventTreeSummary(const QJsonObject &,int row,const SceneEvents *flags=nullptr,const Project *project=nullptr);
QString eventConditionSummary(const QJsonObject &,int row,const SceneEvents *flags=nullptr);
QString eventTargetSummary(int row,int group,int target);
QMap<int,QString> eventTargets(Project *,int row,int group);
}
