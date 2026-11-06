#pragma once
#include "engine/project.h"
#include <optional>
class QWidget;
namespace lordly {
std::optional<Project> editCampaignSetup(const Project &,int location,bool create,QWidget *parent);
}
