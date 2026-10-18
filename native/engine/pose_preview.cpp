#include "engine/pose_preview.h"
#include <QPainter>
#include <QSet>
#include <cmath>

namespace lordly {
QRectF PosePreviewLayer::bounds() const {
    return {position, QSizeF(image.width() * scaleX, image.height() * scaleY)};
}
QVector<PosePreviewLayer> posePreviewLayers(const QVector<ActorFrameLayer> &frame,
                                           double x, double y, const QByteArray &material) {
    QVector<PosePreviewLayer> result;
    for (int i = 0; i < frame.size(); ++i) {
        const auto &layer = frame[i];
        const auto &m = layer.metadata;
        const auto &r = layer.pixels.image;
        result << PosePreviewLayer{
            QImage(reinterpret_cast<const uchar *>(r.rgba.constData()), r.width, r.height,
                   r.width * 4, QImage::Format_RGBA8888).convertToFormat(QImage::Format_ARGB32_Premultiplied),
            {m.origin.x() * m.scaleX / 1024. + x, m.origin.y() * m.scaleY / 1024. - y},
            m.scaleX / 1024., m.scaleY / 1024., i < material.size() ? u8(material, i) / 255. : 1.};
    }
    return result;
}
QImage paintPosePreview(const QVector<PosePreviewLayer> &layers, const QRect &canvas) {
    if (canvas.isEmpty() || canvas.width() > SpriteLibrary::dimensionLimit ||
        canvas.height() > SpriteLibrary::dimensionLimit)
        throw Error("Animation canvas exceeds 4096 pixels per side");
    QImage image(canvas.size(), QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    for (const auto &layer : layers) {
        painter.save();
        painter.translate(layer.position - canvas.topLeft());
        painter.scale(layer.scaleX, layer.scaleY);
        painter.setOpacity(layer.opacity);
        painter.drawImage(QPointF(0, 0), layer.image);
        painter.restore();
    }
    return image;
}
std::optional<SpriteAsset> PosePlaybackSource::capture(const std::function<bool(int)> &progress) const {
    if (!art.isBody() || stepsPerSecond < 1 || stepsPerSecond > 120)
        throw Error("Choose a combat animation to export");
    ActorPosePlayer player(rom, art, state, variant);
    player.setLinkedObjectKnownAbsent(true); // Same boundary as ActorPoseEditor::restart.
    SpriteAsset asset;
    asset.id = SpriteLibrary::newId();
    asset.name = "Combat animation";
    asset.kind = "sequence";
    QRect bounds;
    QSet<QString> visited;
    quint64 pixels = 0;
    int steps = 0;
    bool ended = false;
    QString endReason;
    for (; steps < 1800; ++steps) {
        if (progress && !progress(steps)) return {};
        if (player.advance() != LORDLY_POSE_OK)
            throw Error("Cannot export preview playback: " + player.error());
        const auto &pose = player.actor();
        if (player.result() == 0) { ended = true; endReason = "end"; break; }
        // Repeating the program counter and remaining hold identifies a cycle
        // even when each loop moves or fades the actor further.
        const auto key = QString("%1:%2:%3").arg(pose.state).arg(pose.cursor).arg(pose.delay);
        if (visited.contains(key)) { ended = true; endReason = "cycle"; break; }
        visited.insert(key);
        const auto decoded = art.bodyFrame(rom, pose.frame, appearance.value_or(art.defaultAppearance(rom)),
                                           sepia, artwork);
        const auto layers = posePreviewLayers(decoded, pose.position.x, pose.position.y,
                                              QByteArray(reinterpret_cast<const char *>(pose.material), 16));
        QRectF frameBounds;
        for (const auto &layer : layers) frameBounds |= layer.bounds();
        if (!std::isfinite(frameBounds.x()) || !std::isfinite(frameBounds.y()) ||
            std::abs(frameBounds.x()) > 1000000 || std::abs(frameBounds.y()) > 1000000)
            throw Error("Animation movement exceeds export bounds");
        const QRect rect = frameBounds.isEmpty() ? QRect(0, 0, 1, 1) : frameBounds.toAlignedRect();
        bounds |= rect;
        if (bounds.width() > SpriteLibrary::dimensionLimit || bounds.height() > SpriteLibrary::dimensionLimit)
            throw Error("Animation movement exceeds a 4096-pixel canvas");
        auto image = paintPosePreview(layers, rect);
        if (!asset.frames.isEmpty()) {
            auto &last = asset.frames.last();
            if (last.ticks < 255 && last.layers[0].position == rect.topLeft() && last.layers[0].image == image) {
                ++last.ticks;
                continue;
            }
        }
        pixels += quint64(image.width()) * image.height();
        if (pixels > SpriteLibrary::pixelLimit || asset.frames.size() >= 1024)
            throw Error("Animation export exceeds the frame or memory limit");
        asset.frames << SpriteAssetFrame{SpriteLibrary::newId(), QString("Frame %1").arg(asset.frames.size() + 1), 1,
            {{SpriteLibrary::newId(), "Preview", true, rect.topLeft(), std::move(image)}}};
    }
    if (!ended) throw Error("Animation did not end or repeat within 1800 preview steps");
    if (asset.frames.isEmpty()) throw Error("This animation has no playable frames");
    asset.size = bounds.size();
    asset.anchor = -bounds.topLeft();
    for (auto &frame : asset.frames) frame.layers[0].position -= bounds.topLeft();
    asset.provenance = {{"source", "combat-preview-playback"}, {"classId", art.sourceClass()},
        {"bodyFlags", art.flagA() * 2 + art.flagB()}, {"pose", state},
        {"stepsPerSecond", stepsPerSecond}, {"steps", steps}, {"stoppedAt", endReason}};
    asset.validate();
    return asset;
}
} // namespace lordly
