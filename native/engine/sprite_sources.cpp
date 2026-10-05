#include "engine/sprite_sources.h"
#include <QRectF>
#include <cmath>
#include <cstring>
namespace lordly {
SpriteAsset spriteAssetFromFrames(const QString &name, const QVector<SpriteSourceFrame> &frames) {
    if (frames.isEmpty() || frames.size() > 1024)
        throw Error("Choose a pose with 1–1024 visible frame instructions");
    SpriteAsset result;
    result.id = SpriteLibrary::newId();
    result.name = name;
    result.kind = frames.size() == 1 ? "frame" : "sequence";
    QRect bounds;
    quint64 pixels = 0;
    for (const auto &source : frames) {
        if (source.layers.size() > 64)
            throw Error("Sprite frames support at most 64 layers");
        SpriteAssetFrame frame{SpriteLibrary::newId(),
                               QString("Frame %1").arg(result.frames.size() + 1),
                               source.ticks,
                               {}};
        for (const auto &sourceLayer : source.layers) {
            const auto &m = sourceLayer.metadata;
            const auto &r = sourceLayer.pixels.image;
            if (r.width < 1 || r.height < 1 ||
                quint64(r.width) * r.height * 4 != quint64(r.rgba.size()))
                throw Error("Invalid decoded sprite pixels");
            const double sx = m.scaleX / 1024., sy = m.scaleY / 1024.;
            const QRectF target(m.origin.x() * sx, m.origin.y() * sy, r.width * sx, r.height * sy);
            const auto rect = target.isEmpty() ? QRect(0, 0, 1, 1) : target.toAlignedRect();
            if (rect.width() > SpriteLibrary::dimensionLimit ||
                rect.height() > SpriteLibrary::dimensionLimit ||
                (pixels += quint64(rect.width()) * rect.height()) > SpriteLibrary::pixelLimit)
                throw Error("Scaled sprite layers exceed the library pixel budget");
            QImage image(rect.size(), QImage::Format_RGBA8888);
            image.fill(Qt::transparent);
            if (!target.isEmpty()) {
                // Sample destination pixel centers. Copy RGBA verbatim: painting
                // would premultiply away hidden RGB and round partial opacity.
                for (int y = 0; y < image.height(); ++y) {
                    const int sourceY = int(std::floor((rect.y() + y + .5 - target.y()) / sy));
                    if (sourceY < 0 || sourceY >= r.height)
                        continue;
                    for (int x = 0; x < image.width(); ++x) {
                        const int sourceX = int(std::floor((rect.x() + x + .5 - target.x()) / sx));
                        if (sourceX >= 0 && sourceX < r.width)
                            std::memcpy(image.scanLine(y) + x * 4,
                                        r.rgba.constData() + (sourceY * r.width + sourceX) * 4, 4);
                    }
                }
            }
            frame.layers << SpriteLayer{
                SpriteLibrary::newId(),
                QString("Layer %1 · art %2").arg(frame.layers.size() + 1).arg(m.art), true,
                rect.topLeft(), std::move(image)};
            bounds |= rect;
        }
        if (frame.layers.isEmpty()) {
            frame.layers = SpriteAsset::blank("Empty frame", {1, 1}).frames.first().layers;
            bounds |= QRect(0, 0, 1, 1);
        }
        result.frames << frame;
    }
    result.size = bounds.size();
    result.anchor = -bounds.topLeft();
    for (auto &frame : result.frames)
        for (auto &layer : frame.layers)
            layer.position -= bounds.topLeft();
    result.validate();
    return result;
}
SpriteAsset spriteAssetFromPose(const Rom &rom, const ActorArt &art, int state, const QString &name,
                                int variant, std::optional<BodyAppearance> appearance,
                                const QMap<int, QByteArray> &artwork) {
    const int requested = state;
    if (state < 0 || state >= art.poses().size())
        throw Error("Choose an available pose");
    while (state < art.poses().size() && art.poses()[state].source.isEmpty())
        ++state;
    if (state >= art.poses().size())
        throw Error("Pose has no frame program");
    if (art.isBody() && !appearance)
        appearance = art.defaultAppearance(rom);
    QVector<SpriteSourceFrame> frames;
    quint64 decodedBytes = 0;
    for (const auto &record : art.poses()[state].records) {
        if (record.opcode != 1 && record.opcode != 21)
            continue;
        const int token =
            u8(record.operands, 0) + (record.opcode == 21 ? 256 * u8(record.operands, 1) : 0);
        auto layers = art.isBody() ? art.bodyFrame(rom, token, *appearance, 0, artwork)
                                   : art.frame(rom, token, variant, 0, {}, artwork);
        for (const auto &layer : layers)
            decodedBytes += layer.pixels.image.rgba.size();
        if (decodedBytes > SpriteLibrary::pixelLimit * 4 || frames.size() >= 1024)
            throw Error("Pose exceeds the sprite library pixel budget");
        frames << SpriteSourceFrame{u8(record.operands, record.operands.size() - 1),
                                    std::move(layers)};
    }
    auto result = spriteAssetFromFrames(name, frames);
    result.provenance = {{"source", "native-game-pose"},
                         {"descriptorKey", double(art.descriptorKey())},
                         {"pose", requested},
                         {"resolvedPose", state},
                         {"frameListOnly", true},
                         {"bank", art.bank()},
                         {"variant", variant}};
    if (art.isBody()) {
        result.provenance["classId"] = art.sourceClass();
        result.provenance["ownerId"] = art.ownerClass();
        result.provenance["bodyFlags"] = art.flagA() * 2 + art.flagB();
        result.provenance["equipment"] = appearance->equipment;
        result.provenance["paletteReplacement"] =
            appearance->paletteReplacement ? int(*appearance->paletteReplacement) : -1;
    }
    return result;
}
} // namespace lordly
