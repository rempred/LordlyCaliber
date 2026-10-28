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
    if (hue < -180 || hue > 180 || color < -1 || color > 0xffffff ||
        sizePercent < 25 || sizePercent > 400 || offsetX < -320 || offsetX > 320 || offsetY < -240 || offsetY > 240 ||
        artwork.size() > 256 || (frames && nativeEdited()))
        throw Error("Invalid retail effect appearance settings");
    quint64 artworkBytes = 0;
    for (auto i = artwork.begin(); i != artwork.end(); ++i) {
        const auto &a = i.value();
        const int type = i.key() >> 16, id = i.key() & 65535;
        if ((type != 4 && type != 7 && type != 8) || !id || a.width < 1 || a.height < 1 ||
            a.width > 256 || a.height > 4096 || a.rgba.size() != qint64(a.width) * a.height * 4 ||
            a.originalHash.size() != 64 || QByteArray::fromHex(a.originalHash).size() != 32 ||
            a.hue < -180 || a.hue > 180 || a.color < -1 || a.color > 0xffffff)
            throw Error("Invalid retail effect artwork");
        artworkBytes += a.rgba.size();
    }
    if (artworkBytes > 512 * 1024)
        throw Error("An edited retail effect may contain up to 512 KiB of replacement artwork");
    if (!frames)
        return;
    frames->validate();
    if (frames->provenance.contains("effectPlacement")) {
        const auto value = frames->provenance["effectPlacement"];
        if (!value.isDouble() || value.toDouble() != framePlacement() || framePlacement() < 0 ||
            framePlacement() > 3)
            throw Error("Invalid effect placement");
    }
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
    if (nativeEdited()) {
        QJsonArray replacements;
        for (auto i = artwork.begin(); i != artwork.end(); ++i)
            replacements.append(QJsonObject{{"key", double(i.key())}, {"width", i->width},
                {"height", i->height}, {"rgba", QString::fromLatin1(i->rgba.toBase64())},
                {"originalHash", QString::fromLatin1(i->originalHash)}, {"hue", i->hue}, {"color", i->color}});
        result["retailEdits"] = QJsonObject{{"hue", hue}, {"color", color},
            {"sizePercent", sizePercent}, {"offsetX", offsetX}, {"offsetY", offsetY},
            {"artwork", replacements}};
    }
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
    if (o.contains("retailEdits")) {
        if (!o["retailEdits"].isObject()) throw Error("Invalid retail effect edits");
        const auto edits = o["retailEdits"].toObject();
        auto value = [&](const char *key, int fallback) {
            if (!edits.contains(key)) return fallback;
            const auto v = edits[key];
            if (!v.isDouble() || !std::isfinite(v.toDouble()) || v.toDouble() != v.toInt())
                throw Error("Invalid retail effect setting");
            return v.toInt();
        };
        result.hue = value("hue", 0); result.color = value("color", -1);
        result.sizePercent = value("sizePercent", 100);
        result.offsetX = value("offsetX", 0); result.offsetY = value("offsetY", 0);
        if (edits.contains("artwork") && !edits["artwork"].isArray()) throw Error("Invalid effect artwork list");
        for (const auto &entry : edits["artwork"].toArray()) {
            const auto a = entry.toObject();
            const auto key = a["key"].toDouble(-1);
            if (!std::isfinite(key) || key < 0 || key > 0xffffffffULL || key != quint32(key) ||
                result.artwork.contains(quint32(key))) throw Error("Invalid effect artwork ID");
            for (const auto field : {"width", "height"})
                if (!a[field].isDouble() || a[field].toDouble() != a[field].toInt())
                    throw Error("Invalid effect artwork dimensions");
            EffectArtwork art{a["width"].toInt(), a["height"].toInt(),
                QByteArray::fromBase64(a["rgba"].toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors),
                a["originalHash"].toString().toLatin1()};
            for (const auto field : {"hue", "color"})
                if (a.contains(field) && (!a[field].isDouble() || a[field].toDouble() != a[field].toInt()))
                    throw Error("Invalid artwork colour setting");
            art.hue = a["hue"].toInt(0); art.color = a["color"].toInt(-1);
            result.artwork[quint32(key)] = art;
        }
    }
    if (o.contains("frames")) {
        if (!o["frames"].isObject())
            throw Error("Invalid effect frames");
        result.frames = SpriteAsset::fromAssetFile(o["frames"].toObject());
    }
    result.validate();
    return result;
}
QByteArray AttackVisual::gameImage(const QImage &source, bool rgba32) {
    const auto image = imageToSrgb(source).convertToFormat(QImage::Format_RGBA8888);
    if (image.isNull() || image.width() > 256 || image.height() > 256)
        throw Error("Invalid effect image dimensions");
    const int stride = ((image.width() + 3) & ~3) * (rgba32 ? 4 : 2);
    QByteArray data(8 + stride * image.height(), 0);
    put16(data, 0, 0x3634);
    put8(data, 2, 0);
    put8(data, 3, rgba32 ? 3 : 2);
    put16(data, 4, image.width());
    put16(data, 6, image.height());
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            const auto *p = image.constScanLine(y) + x * 4;
            if (rgba32) {
                for (int c = 0; c < 4; ++c) put8(data, 8 + y * stride + x * 4 + c, p[c]);
            } else put16(data, 8 + y * stride + x * 2,
                  quint16((p[0] >> 3) << 11 | (p[1] >> 3) << 6 | (p[2] >> 3) << 1 | (p[3] >= 128)));
        }
    return data;
}
QByteArray AttackVisual::build(int inherited) const {
    validate();
    if (inherited < 0 || inherited > 132)
        throw Error("Invalid inherited attack effect");
    const bool native = nativeEdited();
    QByteArray bytes(native ? 96 : 32, 0);
    bytes.replace(0, 8, native ? "LCFX0002" : "LCFX0001");
    put16(bytes, 12, frames ? 1 : 0);
    put16(bytes, 14, nativeEffect < 0 ? inherited : nativeEffect);
    put16(bytes, 16, quint16(nativeEffect));
    put16(bytes, 18, delay);
    if (native) {
        put16(bytes, 32, color >= 0 ? 2 : hue ? 1 : 0);
        put16(bytes, 34, quint16(hue)); put32(bytes, 36, quint32(color));
        put16(bytes, 40, quint16(sizePercent)); put16(bytes, 42, quint16(offsetX));
        put16(bytes, 44, quint16(offsetY)); put16(bytes, 46, quint16(artwork.size()));
        // F3DEX2 viewports use quarter-pixel coordinates. Keep Z unchanged.
        for (int offset : {64, 80}) {
            put16(bytes, offset, quint16(offset == 64 ? 640 * sizePercent / 100 : 640));
            put16(bytes, offset + 2, quint16(offset == 64 ? 480 * sizePercent / 100 : 480));
            put16(bytes, offset + 4, 511);
            put16(bytes, offset + 8, quint16(640 + (offset == 64 ? offsetX * 4 : 0)));
            put16(bytes, offset + 10, quint16(480 + (offset == 64 ? offsetY * 4 : 0)));
            put16(bytes, offset + 12, 511);
        }
        bytes += QByteArray(artwork.size() * 64, 0);
        int row = 96;
        for (auto i = artwork.begin(); i != artwork.end(); ++i, row += 64) {
            put32(bytes, row, i.key());
            put16(bytes, row + 4, quint16(i->width)); put16(bytes, row + 6, quint16(i->height));
            put32(bytes, row + 8, quint32(bytes.size())); put32(bytes, row + 12, quint32(i->rgba.size()));
            bytes.replace(row + 16, 32, QByteArray::fromHex(i->originalHash));
            put16(bytes, row + 48, quint16(i->hue));
            // Zero keeps the earlier format's reserved bytes/default colours.
            put32(bytes, row + 52, quint32(i->color + 1));
            bytes += i->rgba;
            bytes += QByteArray((16 - bytes.size() % 16) % 16, 0);
        }
    }
    if (frames) {
        put16(bytes, 20, frames->size.width());
        put16(bytes, 22, frames->size.height());
        put16(bytes, 24, quint16(frames->anchor.x()));
        put16(bytes, 26, quint16(frames->anchor.y()));
        put16(bytes, 28, quint16(frames->frames.size()));
        put16(bytes, 30, (fullColorFrames() ? 3 : 0) | (framePlacement() << 8));
        bytes += QByteArray(frames->frames.size() * 16, 0);
        QMap<QByteArray, QPair<quint32, quint32>> images;
        for (int i = 0; i < frames->frames.size(); ++i) {
            const auto raw = gameImage(frames->composite(i), fullColorFrames());
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
        const auto editable = QJsonDocument(frames->assetFile()).toJson(QJsonDocument::Compact);
        if (editable.size() > 64 * 1024 * 1024) throw Error("Editable effect metadata exceeds 64 MiB");
        const auto metadata = qCompress(editable, 9);
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
    const bool native = bytes.startsWith("LCFX0002");
    if (bytes.size() < (native ? 96 : 32) || (!native && !bytes.startsWith("LCFX0001")) ||
        u32(bytes, 8) != quint32(bytes.size()) || u16(bytes, 12) > 1 || u16(bytes, 14) > 132)
        throw Error("Invalid exported attack effect");
    AttackVisual result;
    result.nativeEffect = qint16(u16(bytes, 16));
    result.delay = u16(bytes, 18);
    if (native) {
        if (u16(bytes, 12) || u16(bytes, 32) > 2 || 96 + u16(bytes, 46) * 64 > bytes.size())
            throw Error("Invalid retail effect configuration");
        result.hue = qint16(u16(bytes, 34)); result.color = qint32(u32(bytes, 36));
        result.sizePercent = u16(bytes, 40); result.offsetX = qint16(u16(bytes, 42));
        result.offsetY = qint16(u16(bytes, 44));
        for (int i = 0; i < u16(bytes, 46); ++i) {
            const int row = 96 + i * 64;
            const auto key = u32(bytes, row);
            const auto at = u32(bytes, row + 8), size = u32(bytes, row + 12);
            if (at < quint32(96 + u16(bytes, 46) * 64) || quint64(at) + size > quint64(bytes.size()) ||
                result.artwork.contains(key)) throw Error("Effect artwork lies outside its allocation");
            result.artwork[key] = {u16(bytes, row + 4), u16(bytes, row + 6), bytes.mid(at, size),
                bytes.mid(row + 16, 32).toHex(), qint16(u16(bytes, row + 48)),
                int(qint64(u32(bytes, row + 52)) - 1)};
        }
    }
    if (u16(bytes, 12)) {
        const int w = u16(bytes, 20), h = u16(bytes, 22), count = u16(bytes, 28);
        const int flags = u16(bytes, 30), format = flags & 255, placement = flags >> 8;
        if ((format != 0 && format != 3) || placement > 3 || w < 1 || h < 1 || w > 256 || h > 256 ||
            count < 1 || count > 300 || 32 + count * 16 > bytes.size())
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
                rawSize != quint32(8 + ((w + 3) & ~3) * (format == 3 ? 4 : 2) * h))
                throw Error("Exported effect frame lies outside its allocation");
            if (!images.contains(at)) {
                const auto raw = decodeLzss(bytes.mid(at, size), rawSize);
                if (u8(raw, 2) != 0 || u8(raw, 3) != (format == 3 ? 3 : 2)) throw Error("Effect image format mismatch");
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
        if (result.framePlacement() != placement || result.fullColorFrames() != (format == 3) ||
            result.frames->size != QSize(w, h) || result.frames->frames.size() != count ||
            result.frames->anchor != asset.anchor)
            throw Error("Editable effect metadata disagrees with its game frames");
    }
    result.validate();
    if (native && result.build(u16(bytes, 14)) != bytes)
        throw Error("Retail effect data disagrees with its editable settings");
    return result;
}
} // namespace lordly
