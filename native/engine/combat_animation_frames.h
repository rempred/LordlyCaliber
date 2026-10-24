#pragma once
#include "engine/project.h"
namespace lordly {
struct CombatAnimationFrameTarget {
    int classId = 2, flags = 0, pose = 0, record = -1;
    bool replace = false;
};
// Insert after record, or after the final frame when record is -1. Replacement
// accepts exactly one source frame and retains the destination frame's duration.
// Asset timing uses 30 Hz preview steps; native records retain their game ticks.
Project importCombatAnimationFrames(Project, CombatAnimationFrameTarget, const SpriteAsset &,
                                    const QVector<int> &sourceFrames, bool dither = false);
Project copyCombatAnimationFrames(Project, CombatAnimationFrameTarget, int sourceClass,
                                  int sourceFlags, int sourcePose,
                                  const QVector<int> &sourceRecords);
} // namespace lordly
