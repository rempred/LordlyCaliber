#include "engine/scene_background_renderer.h"
#include "game/images/sepia.h"
namespace lordly {
namespace {
SceneBackgroundChannel loadChannel(const Rom &rom, const QString &id, bool world) {
    SceneBackgroundChannel channel;
    channel.imageId = id;
    const auto &asset = SceneImages::catalog()[id];
    const auto decoded = SceneImages::decode(rom, id);
    QVector<ImageLayer> records;
    if (asset.container == "bg2")
        records = decoded.layers;
    else if (asset.container == "64" || (world && (asset.container == "embedded-njpg" ||
                                                   asset.container == "section-c-njpg"))) {
        const auto &r = decoded.composite;
        records = {{world ? QPoint(-(r.width / 2), -(r.height / 2)) : QPoint(-160, -120), r}};
    } else
        throw Error("This scene background requires its native drawing owner");
    if (records.isEmpty())
        throw Error("Background has no native image records");
    for (const auto &record : records) {
        const auto &r = record.image;
        auto image = QImage(reinterpret_cast<const uchar *>(r.rgba.constData()), r.width, r.height,
                            r.width * 4, QImage::Format_RGBA8888)
                         .copy();
        if (image.isNull())
            throw Error("Unable to allocate a background record image");
        // The world non-B5 vertex builder truncates half-width before mirroring.
        const int width = world && asset.container != "bg2" ? (r.width / 2) * 2 : r.width;
        channel.records << SceneBackgroundRecord{record.origin, std::move(image),
                                                 QSize(width, r.height), decoded.colorFormat};
    }
    return channel;
}
void checkBudget(const QVector<SceneBackgroundChannel> &channels) {
    qsizetype retained = 0;
    for (const auto &channel : channels)
        for (const auto &record : channel.records)
            retained += record.image.sizeInBytes();
    if (retained > 64 * 1024 * 1024)
        throw Error("Scene backgrounds exceed the image budget");
}
} // namespace

SceneBackgroundRenderer SceneBackgroundRenderer::group(const Rom &rom, int selector) {
    const auto members = SceneImages::groupMembers(rom, selector);
    if (members.size() > 20)
        throw Error("Scene group exceeds its twenty background channels");
    SceneBackgroundRenderer result;
    for (const auto &member : members) {
        SceneBackgroundChannel channel;
        if (member.key) {
            if (member.images.size() != 1)
                throw Error("Scene group member requires its native non-image owner");
            channel.imageId = member.images.first();
            channel = loadChannel(rom, channel.imageId, false);
        }
        result.channels_ << std::move(channel);
        checkBudget(result.channels_);
    }
    return result;
}
SceneBackgroundRenderer SceneBackgroundRenderer::world(const Rom &rom, int environment,
                                                       int foreground) {
    SceneBackgroundRenderer result;
    for (const auto &images :
         {SceneImages::environment(rom, environment), SceneImages::foreground(rom, foreground)}) {
        if (images.size() > 1)
            throw Error("Ambiguous world background image");
        result.channels_ << (images.isEmpty() ? SceneBackgroundChannel{}
                                              : loadChannel(rom, images.first(), true));
        checkBudget(result.channels_);
    }
    return result;
}
const SceneBackgroundChannel &SceneBackgroundRenderer::channel(int index) const {
    if (index < 0 || index >= channels_.size())
        throw Error("Background channel is outside its group");
    return channels_[index];
}
QVector<ActorRasterLayer> SceneBackgroundRenderer::project(int index, const SceneCamera &camera,
                                                           const QByteArray &parent,
                                                           RenderColor primitive) const {
    const auto &source = channel(index);
    return project(source, index, camera, parent, primitive);
}
QVector<ActorRasterLayer> SceneBackgroundRenderer::project(const SceneBackgroundChannel &source,
                                                           int index, const SceneCamera &camera,
                                                           const QByteArray &parent,
                                                           RenderColor primitive) {
    if (parent.size() != 64)
        throw Error("Background projection requires a packed parent matrix");
    QVector<ActorRasterLayer> result;
    for (const auto &record : source.records) {
        const auto &r = record.image;
        ActorRasterLayer layer;
        layer.slot = index;
        layer.layer = result.size();
        // Native B5 records describe full extents, unlike inclusive Actor vertices.
        const double originX = record.origin.x(), originY = -record.origin.y();
        layer.plane = camera.projectPackedPlane(
            parent, {originX, originY, 0}, {double(record.extent.width()) / r.width(), 0, 0},
            {0, -double(record.extent.height()) / r.height(), 0}, QSizeF(r.width(), r.height()));
        layer.image = r; // Implicitly shared with immutable group/snapshot storage.
        layer.primitive = primitive;
        if (primitive != RenderColor{255, 255, 255, 255}) {
            layer.image = r.convertToFormat(QImage::Format_RGBA8888);
            for (int y = 0; y < layer.image.height(); ++y) {
                auto pixels = layer.image.scanLine(y); // Detaches before mutation.
                for (int x = 0; x < layer.image.width(); ++x)
                    for (int c = 0; c < 4; ++c)
                        pixels[x * 4 + c] = quint8(int(pixels[x * 4 + c]) * primitive[c] / 255);
            }
        }
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
void SceneBackgroundRenderer::convertToSepia() {
    for (auto &channel : channels_)
        convertToSepia(channel);
}
void SceneBackgroundRenderer::convertToSepia(SceneBackgroundChannel &channel) {
    for (auto &record : channel.records) {
        if (record.colorFormat != 2 && record.colorFormat != 3)
            continue;
        for (int y = 0; y < record.image.height(); ++y) {
            auto p = record.image.scanLine(y);
            for (int x = 0; x < record.image.width(); ++x, p += 4) {
                if (record.colorFormat == 3) {
                    const auto word = lordly_sepia_rgba32(
                        quint32(p[0]) << 24 | quint32(p[1]) << 16 | quint32(p[2]) << 8 | p[3]);
                    p[0] = quint8(word >> 24);
                    p[1] = quint8(word >> 16);
                    p[2] = quint8(word >> 8);
                } else {
                    const auto word = lordly_sepia_rgba16(
                        quint16((p[0] >> 3) << 11 | (p[1] >> 3) << 6 | (p[2] >> 3) << 1 | 1));
                    for (int c = 0; c < 3; ++c)
                        p[c] = quint8((((word >> (11 - c * 5)) & 31) * 255 + 15) / 31);
                }
            }
        }
    }
}
} // namespace lordly
