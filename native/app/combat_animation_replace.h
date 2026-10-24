#pragma once
#include "engine/combat_animation_edit.h"
#include <optional>
class QWidget;
namespace lordly {
std::optional<Project> chooseCombatAnimationReplacement(Project, CombatAnimationTarget, bool create,
                                                        QWidget *);
std::optional<Project> chooseCombatAnimationFrames(Project, int classId, int flags, int pose,
                                                   int destinationRecord, bool replace, QWidget *);
} // namespace lordly
