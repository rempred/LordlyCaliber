#pragma once
#include "engine/project.h"
class QWidget;
class QUndoStack;
namespace lordly {
void replaceCampaignBackground(QWidget *,Project *,QUndoStack *);
void editCampaignIcons(QWidget *,Project *,QUndoStack *,int selected);
}
