#pragma once
#include "rebuild/export_plan.h"
namespace lordly {
ExportPlan::Ranges exportOwners(const QString &step);
void configureExportComposition(ExportPlan &plan);
}
