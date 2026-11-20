#pragma once
#include "rebuild/compiler.h"
#include "rebuild/export_plan.h"
namespace lordly {
void validateUserPatches(const QJsonValue &value);
void validateModuleSource(const QString &source, const QMap<QString, QByteArray> &headers);
void addUserPatches(ExportPlan &plan, const QJsonArray &patches, const Compiler &compiler,
                    const QString &directory, QStringList &report);
} // namespace lordly
