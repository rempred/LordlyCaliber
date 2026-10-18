#pragma once
#include "engine/actor_pose.h"
#include "engine/sprite_library.h"
#include <functional>

namespace lordly {
// Shared by the flat preview and animation capture. Guides, onion skins and
// selection highlighting are editor overlays, not part of the rendered artwork.
struct PosePreviewLayer {
    QImage image;
    QPointF position;
    double scaleX = 1, scaleY = 1, opacity = 1;
    QRectF bounds() const;
};
QVector<PosePreviewLayer> posePreviewLayers(const QVector<ActorFrameLayer> &,
                                          double x, double y, const QByteArray &material);
QImage paintPosePreview(const QVector<PosePreviewLayer> &, const QRect &canvas);
struct PosePlaybackSource {
    Rom rom;
    ActorArt art;
    int state = 0, variant = 0, stepsPerSecond = 30;
    bool sepia = false;
    std::optional<BodyAppearance> appearance;
    QMap<int, QByteArray> artwork;
    // A fresh copy of the preview interpreter, stopping at end or the first
    // repeated control step. No changes to live playback or project state.
    std::optional<SpriteAsset> capture(const std::function<bool(int)> &progress = {}) const;
};
} // namespace lordly
