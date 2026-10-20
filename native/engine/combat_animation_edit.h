#pragma once
#include "engine/project.h"
namespace lordly {
struct CombatAnimationTarget {
    int classId = 2, flags = 0, action = -1;
    bool blocked = false;
};
struct NamedCombatAnimation {
    QString name;
    int action, pose;
    bool blocked;
};
QMap<int, int> combatMotions();
QMap<int, QString> combatMotionNames();
QVector<NamedCombatAnimation> namedCombatAnimations(Project &, int classId, int flags);
int combatAnimationPose(Project &, CombatAnimationTarget);
QString combatAnimationName(Project &, CombatAnimationTarget);
Project replaceCombatAnimation(Project, CombatAnimationTarget, const SpriteAsset &, bool loop,
                               bool dither = false);
Project copyCombatAnimation(Project, CombatAnimationTarget, CombatAnimationTarget source);
} // namespace lordly
