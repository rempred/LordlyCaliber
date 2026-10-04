#include "engine/scene_background_renderer.h"
namespace lordly {
SceneBackgroundRenderer SceneBackgroundRenderer::group(const Rom &rom, int selector) {
    const auto members = SceneImages::groupMembers(rom, selector);
    if (members.size() > 20)
        throw Error("Scene group exceeds its twenty background channels");
    SceneBackgroundRenderer result;
    qsizetype retained = 0;
    for (const auto &member : members) {
        SceneBackgroundChannel channel;
        if (member.key) {
            if (member.images.size() != 1)
                throw Error("Scene group member requires its native non-image owner");
            channel.imageId = member.images.first();
            const auto &asset = SceneImages::catalog()[channel.imageId];
            const auto decoded = SceneImages::decode(rom, channel.imageId);
            QVector<ImageLayer> records;
            if (asset.container == "bg2")
                records = decoded.layers;
            else if (asset.container == "64")
                // ROM1FAC5C dispatch fixes the origin, regardless of image size.
                records = {{QPoint(-160, -120), decoded.composite}};
            else
                throw Error("This scene background requires its native drawing owner");
            if (records.isEmpty())
                throw Error("Background has no native image records");
            for (const auto &record : records) {
                const auto &r = record.image;
                retained += r.rgba.size();
                if (retained > 64 * 1024 * 1024)
                    throw Error("Scene backgrounds exceed the image budget");
                auto image = QImage(reinterpret_cast<const uchar *>(r.rgba.constData()), r.width,
                                    r.height, r.width * 4, QImage::Format_RGBA8888)
                                 .copy();
                if (image.isNull())
                    throw Error("Unable to allocate a background record image");
                channel.records << SceneBackgroundRecord{record.origin, std::move(image)};
            }
        }
        result.channels_ << std::move(channel);
    }
    return result;
}
const SceneBackgroundChannel &SceneBackgroundRenderer::channel(int index) const {
    if (index < 0 || index >= channels_.size())
        throw Error("Background channel is outside its group");
    return channels_[index];
}
QVector<ActorRasterLayer> SceneBackgroundRenderer::project(int index, const SceneCamera &camera,
                                                           const QByteArray &parent) const {
    const auto &source = channel(index);
    if (parent.size() != 64)
        throw Error("Background projection requires a packed parent matrix");
    QVector<ActorRasterLayer> result;
    for (const auto &record : source.records) {
        const auto &r = record.image;
        ActorRasterLayer layer;
        layer.slot = index;
        layer.layer = result.size();
        // Native B5 records describe full extents, unlike inclusive Actor vertices.
        const double x = record.origin.x(), y = -record.origin.y();
        layer.plane = camera.projectPackedPlane(parent, {x, y, 0}, {1, 0, 0}, {0, -1, 0},
                                                QSizeF(r.width(), r.height()));
        layer.image = r; // Implicitly shared with immutable group/snapshot storage.
        result << std::move(layer);
    }
    return result;
}
QString SceneBackgroundRenderer::description() const {
    QStringList names;
    for (int i = 0; i < channels_.size(); ++i)
        if (!channels_[i].imageId.isEmpty())
            names << QString("%1: %2").arg(i).arg(channels_[i].imageId);
    return names.join(", ");
}
} // namespace lordly
