#include "engine/attack_visual.h"
#include "engine/image.h"
#include "engine/image_colors.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <cmath>
namespace lordly {
void AttackVisual::validate() const {
    if (nativeEffect < -1 || nativeEffect > 132 || delay < 0 || delay > 300)
        throw Error("Choose a retail effect and a delay between 0 and 300 ticks");
    if (!frames)
        return;
    frames->validate();
    if (frames->size.width() > 256 || frames->size.height() > 256 || frames->frames.size() > 300 ||
        frames->pixels() > 16 * 1024 * 1024)
        throw Error("An imported effect may use up to 300 frames, 256 × 256 pixels, and 16 million "
                    "frame pixels");
    if (std::abs(frames->anchor.x()) > 1024 || std::abs(frames->anchor.y()) > 1024)
        throw Error("The effect origin must be within 1024 pixels of its canvas");
    int ticks = 0;
    for (const auto &f : frames->frames) {
        if (f.ticks < 1 || f.ticks > 255)
            throw Error("Each effect frame must last between 1 and 255 ticks");
        ticks += f.ticks;
    }
    if (ticks < 1 || ticks > 900)
        throw Error("An imported effect must last between 1 and 900 ticks (30 seconds)");
}
QJsonObject AttackVisual::json() const {
    validate();
    QJsonObject result{{"nativeEffect", nativeEffect}, {"delay", delay}};
    if (frames)
        result["frames"] = frames->assetFile();
    return result;
}
AttackVisual AttackVisual::fromJson(const QJsonObject &o) {
    AttackVisual result;
    auto integer = [&](const char *key, int fallback) {
        if (!o.contains(key))
            return fallback;
        const auto v = o[key];
        if (!v.isDouble() || !std::isfinite(v.toDouble()) || v.toDouble() != v.toInt())
            throw Error("Invalid effect setting");
        return v.toInt();
    };
    result.nativeEffect = integer("nativeEffect", -1);
    result.delay = integer("delay", 0);
    if (o.contains("frames")) {
        if (!o["frames"].isObject())
            throw Error("Invalid effect frames");
        result.frames = SpriteAsset::fromAssetFile(o["frames"].toObject());
    }
    result.validate();
    return result;
}
QByteArray AttackVisual::gameImage(const QImage &source) {
    const auto image = imageToSrgb(source).convertToFormat(QImage::Format_RGBA8888);
    if (image.isNull() || image.width() > 256 || image.height() > 256)
        throw Error("Invalid effect image dimensions");
    const int stride = (image.width() * 2 + 7) & ~7;
    QByteArray data(8 + stride * image.height(), 0);
    put16(data, 0, 0x3634);
    put8(data, 2, 0);
    put8(data, 3, 2);
    put16(data, 4, image.width());
    put16(data, 6, image.height());
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            const auto *p = image.constScanLine(y) + x * 4;
            put16(data, 8 + y * stride + x * 2,
                  quint16((p[0] >> 3) << 11 | (p[1] >> 3) << 6 | (p[2] >> 3) << 1 | (p[3] >= 128)));
        }
    return data;
}
QByteArray AttackVisual::build(int inherited) const {
    validate();
    if (inherited < 0 || inherited > 132)
        throw Error("Invalid inherited attack effect");
    QByteArray bytes(32, 0);
    bytes.replace(0, 8, "LCFX0001");
    put16(bytes, 12, frames ? 1 : 0);
    put16(bytes, 14, nativeEffect < 0 ? inherited : nativeEffect);
    put16(bytes, 16, quint16(nativeEffect));
    put16(bytes, 18, delay);
    if (frames) {
        put16(bytes, 20, frames->size.width());
        put16(bytes, 22, frames->size.height());
        put16(bytes, 24, quint16(frames->anchor.x()));
        put16(bytes, 26, quint16(frames->anchor.y()));
        put16(bytes, 28, quint16(frames->frames.size()));
        bytes += QByteArray(frames->frames.size() * 16, 0);
        QMap<QByteArray, QPair<quint32, quint32>> images;
        for (int i = 0; i < frames->frames.size(); ++i) {
            const auto raw = gameImage(frames->composite(i));
            const auto hash = sha256(raw);
            if (!images.contains(hash)) {
                const auto compressed = encodeLzss(raw);
                images[hash] = {quint32(bytes.size()), quint32(compressed.size())};
                bytes += compressed;
            }
            const auto [at, size] = images[hash];
            const int row = 32 + i * 16;
            put32(bytes, row, at);
            put32(bytes, row + 4, size);
            put16(bytes, row + 8, quint16(frames->frames[i].ticks));
            put32(bytes, row + 12, quint32(raw.size()));
        }
    }
    if (frames) {
        const auto metadata =
            qCompress(QJsonDocument(frames->assetFile()).toJson(QJsonDocument::Compact), 9);
        const auto at = quint32(bytes.size());
        bytes += metadata;
        bytes += QByteArray((16 - bytes.size() % 16) % 16, 0);
        QByteArray trailer(16, 0);
        trailer.replace(0, 8, "FXMETA01");
        put32(trailer, 8, at);
        put32(trailer, 12, quint32(metadata.size()));
        bytes += trailer;
    } else
        bytes += QByteArray((16 - bytes.size() % 16) % 16, 0);
    put32(bytes, 8, quint32(bytes.size()));
    return bytes;
}
AttackVisual AttackVisual::read(const QByteArray &bytes) {
    if (bytes.size() < 32 || !bytes.startsWith("LCFX0001") ||
        u32(bytes, 8) != quint32(bytes.size()) || u16(bytes, 12) > 1 || u16(bytes, 14) > 132)
        throw Error("Invalid exported attack effect");
    AttackVisual result;
    result.nativeEffect = qint16(u16(bytes, 16));
    result.delay = u16(bytes, 18);
    if (u16(bytes, 12)) {
        const int w = u16(bytes, 20), h = u16(bytes, 22), count = u16(bytes, 28);
        if (w < 1 || h < 1 || w > 256 || h > 256 || count < 1 || count > 300 ||
            32 + count * 16 > bytes.size())
            throw Error("Invalid exported effect dimensions");
        SpriteAsset asset = SpriteAsset::blank("Imported attack effect", {w, h});
        asset.frames.clear();
        asset.kind = "sequence";
        asset.anchor = {qint16(u16(bytes, 24)), qint16(u16(bytes, 26))};
        QMap<quint32, QImage> images;
        for (int i = 0; i < count; ++i) {
            const int row = 32 + i * 16;
            const auto at = u32(bytes, row), size = u32(bytes, row + 4),
                       rawSize = u32(bytes, row + 12);
            if (at < quint32(32 + count * 16) || quint64(at) + size > quint64(bytes.size()) ||
                rawSize != quint32(8 + ((w * 2 + 7) & ~7) * h))
                throw Error("Exported effect frame lies outside its allocation");
            if (!images.contains(at)) {
                const auto raw = decodeLzss(bytes.mid(at, size), rawSize);
                const auto decoded = decodeN64Image(raw);
                if (decoded.width != w || decoded.height != h)
                    throw Error("Exported effect frame has inconsistent dimensions");
                images[at] = QImage(reinterpret_cast<const uchar *>(decoded.rgba.constData()), w, h,
                                    w * 4, QImage::Format_RGBA8888)
                                 .copy();
            }
            SpriteAssetFrame frame;
            frame.id = QString("frame-%1").arg(i);
            frame.name = QString("Frame %1").arg(i + 1);
            frame.ticks = u16(bytes, row + 8);
            frame.layers.append({QString("layer-%1").arg(i), "Effect", true, {}, images[at]});
            asset.frames.append(frame);
        }
        const auto trailer = bytes.last(16);
        if (!trailer.startsWith("FXMETA01"))
            throw Error("Missing editable effect frame metadata");
        const auto at = u32(trailer, 8), size = u32(trailer, 12);
        if (size < 4 || at < quint32(32 + count * 16) ||
            quint64(at) + size > quint64(bytes.size() - 16) || u32(bytes, at) > 64 * 1024 * 1024)
            throw Error("Invalid editable effect frame metadata");
        const auto metadata = qUncompress(bytes.mid(at, size));
        const auto document = QJsonDocument::fromJson(metadata);
        if (!document.isObject())
            throw Error("Invalid editable effect frame document");
        result.frames = SpriteAsset::fromAssetFile(document.object());
        if (result.frames->size != QSize(w, h) || result.frames->frames.size() != count ||
            result.frames->anchor != asset.anchor)
            throw Error("Editable effect metadata disagrees with its game frames");
    }
    result.validate();
    return result;
}
} // namespace lordly
