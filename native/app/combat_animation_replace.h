#pragma once
#include "engine/combat_animation_edit.h"
#include <optional>
class QWidget;
namespace lordly {
std::optional<Project> chooseCombatAnimationReplacement(Project, CombatAnimationTarget, bool create,
                                                        QWidget *);
}
