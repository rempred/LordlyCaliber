#pragma once
#include "engine/actor_art.h"
#include <QSet>
namespace lordly {
// Copy art, timing and optionally the donor's control flow into an independent
// destination pose. Equipment sprites keep their native selection policies.
AnimationDescriptor copyAnimationPose(const Rom &, AnimationDescriptor destination, int destinationPose,
                                      const AnimationDescriptor &source, const ActorArt &, int sourcePose,
                                      bool copyControls);
AnimationDescriptor copyAnimationFrame(const Rom &, AnimationDescriptor destination, int destinationPose,
                                       int destinationRecord, const AnimationDescriptor &source,
                                       const ActorArt &, int sourcePose, int sourceRecord, bool copyDuration);
AnimationDescriptor translateAnimationFrames(AnimationDescriptor, const QMap<int, QSet<int>> &records,
                                               QPoint offset, int layer = -1);
} // namespace lordly
