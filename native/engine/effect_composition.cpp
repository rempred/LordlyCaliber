#include "engine/effect_composition.h"
#include "engine/attack_visual.h"
#include <QColorSpace>
#include <QJsonArray>
#include <QPainter>
#include <QSet>
#include <algorithm>
#include <cmath>
namespace lordly {
namespace {
int number(const QJsonObject &o, const char *field, int fallback) {
    if (!o.contains(field))
        return fallback;
    const auto v = o[field];
    if (!v.isDouble() || !std::isfinite(v.toDouble()) || v.toDouble() != v.toInt())
        throw Error("Invalid effect piece setting: " + QString::fromLatin1(field));
    return v.toInt();
}
} // namespace
void EffectComposition::validate() const {
    if (id.isEmpty() || id.size() > 120 || name.trimmed().isEmpty() || name.size() > 120 ||
        size.width() < 1 || size.height() < 1 || size.width() > 256 || size.height() > 240 ||
        duration < 1 || duration > 300 || pieces.size() > 24 || placement < 0 || placement > 3)
        throw Error(
            "Effects support 24 pieces, a canvas up to 256 × 240, and 300 ticks (10 seconds)");
    if (qint64(size.width()) * size.height() * duration > 16 * 1024 * 1024)
        throw Error("This canvas and duration exceed 16 million frame pixels. Reduce the canvas or "
                    "duration.");
    QSet<QString> ids;
    quint64 pixels = 0;
    for (const auto &p : pieces) {
        p.artwork.validate();
        pixels += p.artwork.pixels();
        if (p.id.isEmpty() || p.id.size() > 120 || ids.contains(p.id) ||
            p.name.trimmed().isEmpty() || p.name.size() > 120 || p.start < 0 || p.start > 299 ||
            p.duration < 1 || p.duration > 300 || p.start + p.duration > duration ||
            p.fromScale < 1 || p.fromScale > 400 || p.toScale < 1 || p.toScale > 400 ||
            p.fromAngle < -1080 || p.fromAngle > 1080 || p.toAngle < -1080 || p.toAngle > 1080 ||
            p.fromOpacity < 0 || p.fromOpacity > 100 || p.toOpacity < 0 || p.toOpacity > 100 ||
            p.easing < 0 || p.easing > 3 || std::abs(qint64(p.from.x())) > 512 ||
            std::abs(qint64(p.from.y())) > 512 || std::abs(qint64(p.to.x())) > 512 ||
            std::abs(qint64(p.to.y())) > 512)
            throw Error("Invalid piece timing or movement. Each piece must fit within the effect "
                        "duration.");
        ids.insert(p.id);
    }
    if (pixels > 4 * 1024 * 1024)
        throw Error("Effect source artwork exceeds four million pixels");
}
QImage EffectComposition::render(int tick) const {
    QImage image(size, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    image.setColorSpace(QColorSpace::SRgb);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    for (const auto &p : pieces) {
        if (tick < p.start || tick >= p.start + p.duration)
            continue;
        double t = p.duration <= 1 ? 0 : double(tick - p.start) / (p.duration - 1);
        if (p.easing == 1)
            t *= t;
        else if (p.easing == 2)
            t = 1 - (1 - t) * (1 - t);
        else if (p.easing == 3)
            t = t * t * (3 - 2 * t);
        auto lerp = [t](int a, int b) { return a + (b - a) * t; };
        int total = 0;
        for (const auto &frame : p.artwork.frames)
            total += std::max(frame.ticks, 1);
        int frameTick = tick - p.start;
        frameTick = p.loop ? frameTick % total : std::min(frameTick, total - 1);
        int frame = 0;
        while (frame + 1 < p.artwork.frames.size() &&
               frameTick >= std::max(p.artwork.frames[frame].ticks, 1))
            frameTick -= std::max(p.artwork.frames[frame++].ticks, 1);
        const auto pixels = p.artwork.composite(frame);
        painter.save();
        painter.translate(size.width() / 2.0 + lerp(p.from.x(), p.to.x()),
                          size.height() / 2.0 + lerp(p.from.y(), p.to.y()));
        painter.rotate(lerp(p.fromAngle, p.toAngle));
        const auto scale = lerp(p.fromScale, p.toScale) / 100;
        painter.scale(scale, scale);
        painter.setOpacity(lerp(p.fromOpacity, p.toOpacity) / 100);
        painter.drawImage(QPointF(-pixels.width() / 2.0, -pixels.height() / 2.0), pixels);
        painter.restore();
    }
    return image;
}
QJsonObject EffectComposition::json() const {
    validate();
    QJsonArray list;
    for (const auto &p : pieces)
        list.append(QJsonObject{{"id", p.id},
                                {"name", p.name},
                                {"artwork", p.artwork.assetFile()},
                                {"start", p.start},
                                {"duration", p.duration},
                                {"fromX", p.from.x()},
                                {"fromY", p.from.y()},
                                {"toX", p.to.x()},
                                {"toY", p.to.y()},
                                {"fromScale", p.fromScale},
                                {"toScale", p.toScale},
                                {"fromAngle", p.fromAngle},
                                {"toAngle", p.toAngle},
                                {"fromOpacity", p.fromOpacity},
                                {"toOpacity", p.toOpacity},
                                {"easing", p.easing},
                                {"loop", p.loop}});
    return {{"version", 1},
            {"id", id},
            {"name", name},
            {"width", size.width()},
            {"height", size.height()},
            {"duration", duration},
            {"placement", placement},
            {"pieces", list}};
}
EffectComposition EffectComposition::fromJson(const QJsonObject &o) {
    if (number(o, "version", 0) != 1 || !o["pieces"].isArray() || o["pieces"].toArray().size() > 24)
        throw Error("Unsupported effect composition");
    EffectComposition result;
    result.id = o["id"].toString();
    result.name = o["name"].toString();
    result.size = {number(o, "width", 0), number(o, "height", 0)};
    result.duration = number(o, "duration", 0);
    result.placement = number(o, "placement", 0); // Older compositions stay screen-centred.
    for (const auto &v : o["pieces"].toArray()) {
        if (!v.isObject())
            throw Error("Invalid effect piece");
        const auto a = v.toObject();
        EffectPiece p;
        p.id = a["id"].toString();
        p.name = a["name"].toString();
        p.artwork = SpriteAsset::fromAssetFile(a["artwork"].toObject());
        // Source copies never recursively carry another composition's source images.
        p.artwork.provenance.remove("effectComposition");
        p.start = number(a, "start", 0);
        p.duration = number(a, "duration", 30);
        p.from = {number(a, "fromX", 0), number(a, "fromY", 0)};
        p.to = {number(a, "toX", 0), number(a, "toY", 0)};
        p.fromScale = number(a, "fromScale", 100);
        p.toScale = number(a, "toScale", 100);
        p.fromAngle = number(a, "fromAngle", 0);
        p.toAngle = number(a, "toAngle", 0);
        p.fromOpacity = number(a, "fromOpacity", 100);
        p.toOpacity = number(a, "toOpacity", 100);
        p.easing = number(a, "easing", 0);
        if (a.contains("loop") && !a["loop"].isBool())
            throw Error("Invalid effect repeat setting");
        p.loop = a["loop"].toBool(true);
        result.pieces << p;
    }
    result.validate();
    return result;
}
SpriteAsset EffectComposition::bake() const {
    validate();
    if (pieces.isEmpty())
        throw Error("Add at least one piece to the effect");
    auto asset = SpriteAsset::blank(name, size);
    asset.id = id;
    asset.kind = "sequence";
    asset.anchor = {size.width() / 2, size.height() / 2};
    asset.frames.clear();
    for (int tick = 0; tick < duration; ++tick) {
        const auto image = render(tick);
        if (!asset.frames.isEmpty() && asset.frames.last().ticks < 255 &&
            asset.frames.last().layers[0].image == image) {
            ++asset.frames.last().ticks;
            continue;
        }
        const auto suffix = QString::number(tick);
        asset.frames << SpriteAssetFrame{"frame-" + suffix,
                                         "Frame " + QString::number(tick + 1),
                                         1,
                                         {{"layer-" + suffix, "Effect", true, {}, image}}};
    }
    asset.provenance = {
        {"effectComposition", json()}, {"effectRgba32", true}, {"effectPlacement", placement}};
    AttackVisual visual;
    visual.frames = asset;
    visual.validate();
    return asset;
}
EffectComposition EffectComposition::fromAsset(const SpriteAsset &asset) {
    if (!asset.provenance["effectComposition"].isObject())
        throw Error("This asset has no editable effect pieces");
    auto result = fromJson(asset.provenance["effectComposition"].toObject());
    result.id = asset.id;
    result.name = asset.name;
    return result;
}
} // namespace lordly
