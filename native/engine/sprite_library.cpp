#include "engine/sprite_library.h"
#include <QBuffer>
#include <QImageReader>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
namespace lordly {
namespace {
int integer(const QJsonValue &value, int low, int high, const QString &label) {
    const double n = value.toDouble(std::numeric_limits<double>::quiet_NaN());
    if (!std::isfinite(n) || n != std::floor(n) || n < low || n > high)
        throw Error("Invalid sprite " + label);
    return int(n);
}
QString identifier(const QJsonValue &value) {
    const auto id = value.toString();
    if (!QRegularExpression("^[A-Za-z0-9._:-]{1,120}$").match(id).hasMatch())
        throw Error("Invalid sprite identifier");
    return id;
}
QString cleanName(const QString &value) {
    const auto name = value.simplified();
    if (name.isEmpty() || name.size() > 120)
        throw Error("Sprite names need 1–120 characters");
    return name;
}
void sizeValid(QSize size) {
    if (size.width() < 1 || size.height() < 1 || size.width() > SpriteLibrary::dimensionLimit ||
        size.height() > SpriteLibrary::dimensionLimit)
        throw Error("Sprite dimensions must be 1–4096 pixels");
}
void positionValid(QPoint point) {
    if (point.x() < -32768 || point.x() > 32767 || point.y() < -32768 || point.y() > 32767)
        throw Error("Sprite position exceeds signed 16-bit range");
}
QImage transparent(QSize size) {
    sizeValid(size);
    QImage image(size, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    return image;
}
quint32 pixelWord(const QColor &color) {
    const uchar bytes[]{uchar(color.red()), uchar(color.green()), uchar(color.blue()),
                        uchar(color.alpha())};
    quint32 value;
    std::memcpy(&value, bytes, 4);
    return value;
}
quint32 readPixel(const QImage &image, int x, int y) {
    quint32 result;
    std::memcpy(&result, image.constScanLine(y) + x * 4, 4);
    return result;
}
void writePixel(QImage &image, int x, int y, quint32 pixel) {
    std::memcpy(image.scanLine(y) + x * 4, &pixel, 4);
}
QImage rotate(const QImage &source, bool clockwise) {
    auto out = transparent({source.height(), source.width()});
    for (int y = 0; y < source.height(); ++y)
        for (int x = 0; x < source.width(); ++x)
            writePixel(out, clockwise ? source.height() - 1 - y : y,
                       clockwise ? x : source.width() - 1 - x, readPixel(source, x, y));
    return out;
}
QByteArray rgba(const QImage &image) {
    QByteArray bytes;
    bytes.reserve(image.width() * image.height() * 4);
    for (int y = 0; y < image.height(); ++y)
        bytes.append(reinterpret_cast<const char *>(image.constScanLine(y)), image.width() * 4);
    return bytes;
}
QByteArray decodeBase64(const QJsonValue &value, qsizetype maximum) {
    if (!value.isString())
        throw Error("Sprite pixels are missing");
    const auto text = value.toString();
    if (text.size() > maximum)
        throw Error("Sprite image exceeds its encoded size limit");
    const auto encoded = text.toLatin1();
    const auto result =
        QByteArray::fromBase64Encoding(encoded, QByteArray::AbortOnBase64DecodingErrors);
    if (!result || result.decoded.toBase64() != encoded)
        throw Error("Invalid sprite base64 data");
    return result.decoded;
}
} // namespace
QString SpriteLibrary::newId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
SpriteAsset SpriteAsset::blank(const QString &name, QSize size) {
    return fromImage(name, transparent(size));
}
SpriteAsset SpriteAsset::fromImage(const QString &name, const QImage &image) {
    sizeValid(image.size());
    SpriteAsset result;
    result.id = SpriteLibrary::newId();
    result.name = cleanName(name);
    result.size = image.size();
    result.frames = {{SpriteLibrary::newId(),
                      "Frame 1",
                      8,
                      {{SpriteLibrary::newId(),
                        "Layer 1",
                        true,
                        {},
                        image.convertToFormat(QImage::Format_RGBA8888)}}}};
    return result;
}
quint64 SpriteAsset::pixels() const {
    quint64 result = 0;
    for (const auto &frame : frames)
        for (const auto &layer : frame.layers)
            result += quint64(layer.image.width()) * layer.image.height();
    return result;
}
void SpriteAsset::validate() const {
    identifier(id);
    cleanName(name);
    sizeValid(size);
    positionValid(anchor);
    if (kind != "sprite" && kind != "frame" && kind != "sequence")
        throw Error("Invalid sprite kind");
    if (frames.isEmpty() || frames.size() > 1024 || pixels() > SpriteLibrary::pixelLimit)
        throw Error("Sprite frame count or pixel budget exceeded");
    QSet<QString> frameIds;
    for (const auto &frame : frames) {
        identifier(frame.id);
        cleanName(frame.name);
        if (frameIds.contains(frame.id) || frame.ticks < 0 || frame.ticks > 255 ||
            frame.layers.isEmpty() || frame.layers.size() > 64)
            throw Error("Invalid sprite frame");
        frameIds.insert(frame.id);
        QSet<QString> layerIds;
        for (const auto &layer : frame.layers) {
            identifier(layer.id);
            cleanName(layer.name);
            positionValid(layer.position);
            sizeValid(layer.image.size());
            if (layerIds.contains(layer.id) || layer.image.format() != QImage::Format_RGBA8888)
                throw Error("Invalid sprite layer");
            layerIds.insert(layer.id);
        }
    }
}
SpriteAsset SpriteAsset::fromJson(const QJsonObject &object, int version) {
    if (version < 1 || version > 3)
        throw Error("Unsupported sprite version");
    SpriteAsset result;
    result.id = identifier(object["id"]);
    result.name = cleanName(object["name"].toString());
    result.size = {integer(object["width"], 1, 4096, "width"),
                   integer(object["height"], 1, 4096, "height")};
    if (object.contains("anchor") && !object["anchor"].isObject())
        throw Error("Invalid sprite anchor");
    const auto anchor = object["anchor"].toObject();
    result.anchor = {integer(anchor.value("x").isUndefined() ? QJsonValue(0) : anchor["x"], -32768,
                             32767, "anchor X"),
                     integer(anchor.value("y").isUndefined() ? QJsonValue(0) : anchor["y"], -32768,
                             32767, "anchor Y")};
    if (object.contains("provenance") && !object["provenance"].isObject())
        throw Error("Invalid sprite provenance");
    result.provenance = object["provenance"].toObject();
    const auto frames = object["frames"].toArray();
    if (frames.isEmpty() || frames.size() > 1024)
        throw Error("Invalid sprite frame count");
    quint64 pixels = 0;
    for (const auto &entry : frames) {
        const auto frameObject = entry.toObject();
        SpriteAssetFrame frame;
        frame.id = identifier(frameObject["id"]);
        frame.name = cleanName(frameObject["name"].toString());
        frame.ticks = integer(frameObject["ticks"], 0, 255, "ticks");
        const auto layers = frameObject["layers"].toArray();
        if (layers.isEmpty() || layers.size() > 64)
            throw Error("Invalid sprite layer count");
        for (const auto &layerEntry : layers) {
            const auto layerObject = layerEntry.toObject();
            SpriteLayer layer;
            layer.id = identifier(layerObject["id"]);
            layer.name = cleanName(layerObject["name"].toString());
            if (layerObject.contains("visible") && !layerObject["visible"].isBool())
                throw Error("Invalid layer visibility");
            layer.visible = layerObject.value("visible").toBool(true);
            layer.position = {integer(layerObject.contains("x") ? layerObject["x"] : QJsonValue(0),
                                      -32768, 32767, "layer X"),
                              integer(layerObject.contains("y") ? layerObject["y"] : QJsonValue(0),
                                      -32768, 32767, "layer Y")};
            const QSize size{
                integer(layerObject.contains("width") ? layerObject["width"]
                                                      : QJsonValue(result.size.width()),
                        1, 4096, "layer width"),
                integer(layerObject.contains("height") ? layerObject["height"]
                                                       : QJsonValue(result.size.height()),
                        1, 4096, "layer height")};
            pixels += quint64(size.width()) * size.height();
            if (pixels > SpriteLibrary::pixelLimit)
                throw Error("Sprite exceeds the project pixel budget");
            if (version == 3) {
                auto encoded = decodeBase64(layerObject["pngBase64"], 96 * 1024 * 1024);
                QBuffer buffer(&encoded);
                buffer.open(QIODevice::ReadOnly);
                QImageReader reader(&buffer, "PNG");
                if (reader.size() != size)
                    throw Error("Sprite PNG dimensions disagree with its metadata");
                layer.image = reader.read().convertToFormat(QImage::Format_RGBA8888);
                if (layer.image.isNull())
                    throw Error("Cannot decode sprite PNG");
            } else {
                const qsizetype bytes = qsizetype(size.width()) * size.height() * 4;
                const auto data =
                    decodeBase64(layerObject["pixelsRgbaBase64"], ((bytes + 2) / 3) * 4);
                if (data.size() != bytes)
                    throw Error("Sprite RGBA dimensions disagree with its bytes");
                layer.image = QImage(reinterpret_cast<const uchar *>(data.constData()),
                                     size.width(), size.height(), QImage::Format_RGBA8888)
                                  .copy();
            }
            frame.layers.append(std::move(layer));
        }
        result.frames.append(std::move(frame));
    }
    const auto kind = object["kind"].toString();
    if ((kind != "sprite" && kind != "frame" && kind != "sequence") ||
        (kind != "sequence" && result.frames.size() != 1) ||
        (kind == "sprite" && result.frames.first().layers.size() != 1))
        throw Error("Sprite kind disagrees with its frames/layers");
    result.kind = kind;
    result.validate();
    return result;
}
QString SpriteAsset::effectiveKind() const {
    return frames.size() > 1 || kind == "sequence"               ? "sequence"
           : frames.first().layers.size() > 1 || kind == "frame" ? "frame"
                                                                 : "sprite";
}
QJsonObject SpriteAsset::json(bool browser) const {
    validate();
    QJsonArray frameObjects;
    for (const auto &frame : frames) {
        QJsonArray layers;
        for (const auto &layer : frame.layers) {
            QJsonObject object{{"id", layer.id},
                               {"name", layer.name},
                               {"visible", layer.visible},
                               {"x", layer.position.x()},
                               {"y", layer.position.y()},
                               {"width", layer.image.width()},
                               {"height", layer.image.height()}};
            QByteArray data;
            if (browser)
                data = rgba(layer.image);
            else {
                QBuffer buffer(&data);
                buffer.open(QIODevice::WriteOnly);
                if (!layer.image.save(&buffer, "PNG"))
                    throw Error("Cannot encode sprite PNG");
            }
            object[browser ? "pixelsRgbaBase64" : "pngBase64"] =
                QString::fromLatin1(data.toBase64());
            layers.append(object);
        }
        frameObjects.append(QJsonObject{
            {"id", frame.id}, {"name", frame.name}, {"ticks", frame.ticks}, {"layers", layers}});
    }
    return {{"id", id},
            {"name", name},
            {"kind", effectiveKind()},
            {"width", size.width()},
            {"height", size.height()},
            {"anchor", QJsonObject{{"x", anchor.x()}, {"y", anchor.y()}}},
            {"frames", frameObjects},
            {"provenance", provenance}};
}
QJsonObject SpriteAsset::assetFile(bool browser) const {
    return {{"format", "lordlycaliber-sprite-asset"},
            {"version", browser ? 2 : 3},
            {"asset", json(browser)}};
}
SpriteAsset SpriteAsset::fromAssetFile(const QJsonObject &object) {
    if (object["format"] != "lordlycaliber-sprite-asset" || !object["asset"].isObject())
        throw Error("Not a Lordly Caliber sprite asset file");
    return fromJson(object["asset"].toObject(), integer(object["version"], 1, 3, "file version"));
}
QImage SpriteAsset::composite(int index) const {
    if (index < 0 || index >= frames.size())
        throw Error("Invalid sprite frame index");
    auto out = transparent(size);
    for (const auto &layer : frames[index].layers) {
        if (!layer.visible)
            continue;
        const auto rect = QRect(layer.position, layer.image.size()).intersected(QRect({}, size));
        for (int y = rect.top(); y <= rect.bottom(); ++y)
            for (int x = rect.left(); x <= rect.right(); ++x) {
                const auto src = layer.image.constScanLine(y - layer.position.y()) +
                                 (x - layer.position.x()) * 4;
                if (!src[3])
                    continue;
                auto dst = out.scanLine(y) + x * 4;
                const unsigned sa = src[3], ta = dst[3], den = sa * 255 + ta * (255 - sa);
                for (int c = 0; c < 3; ++c)
                    dst[c] = uchar((src[c] * sa * 255 + dst[c] * ta * (255 - sa) + den / 2) / den);
                dst[3] = uchar((den + 127) / 255);
            }
    }
    return out;
}
QImage spriteScale(const QImage &source, QSize size) {
    if (source.isNull() || source.format() != QImage::Format_RGBA8888)
        throw Error("Invalid sprite scaling image");
    auto out = transparent(size);
    for (int y = 0; y < size.height(); ++y)
        for (int x = 0; x < size.width(); ++x)
            writePixel(out, x, y,
                       readPixel(source, x * source.width() / size.width(),
                                 y * source.height() / size.height()));
    return out;
}
SpriteAsset SpriteAsset::resized(QSize next, bool scale, QPoint shift, bool crop) const {
    sizeValid(next);
    auto result = *this;
    result.size = next;
    const auto sx = [&](int n) {
        return int(std::floor(double(n) * next.width() / size.width() + .5));
    };
    const auto sy = [&](int n) {
        return int(std::floor(double(n) * next.height() / size.height() + .5));
    };
    quint64 pixels = 0;
    for (const auto &frame : frames)
        for (const auto &layer : frame.layers) {
            const QSize target = scale  ? QSize(std::max(1, sx(layer.image.width())),
                                                std::max(1, sy(layer.image.height())))
                                 : crop ? next
                                        : layer.image.size();
            sizeValid(target);
            pixels += quint64(target.width()) * target.height();
        }
    if (pixels > SpriteLibrary::pixelLimit)
        throw Error("Resized sprite exceeds pixel budget");
    for (auto &frame : result.frames)
        for (auto &layer : frame.layers) {
            if (scale) {
                layer.image = spriteScale(layer.image, {std::max(1, sx(layer.image.width())),
                                                        std::max(1, sy(layer.image.height()))});
                layer.position = {sx(layer.position.x()), sy(layer.position.y())};
            } else {
                layer.position += shift;
                if (crop) {
                    auto image = transparent(next);
                    const auto rect =
                        QRect(layer.position, layer.image.size()).intersected(QRect({}, next));
                    for (int y = rect.top(); y <= rect.bottom(); ++y)
                        for (int x = rect.left(); x <= rect.right(); ++x)
                            writePixel(image, x, y,
                                       readPixel(layer.image, x - layer.position.x(),
                                                 y - layer.position.y()));
                    layer.image = std::move(image);
                    layer.position = {};
                }
            }
        }
    result.anchor = scale ? QPoint(sx(anchor.x()), sy(anchor.y())) : anchor + shift;
    result.validate();
    return result;
}
SpriteAsset SpriteAsset::rotated(bool clockwise) const {
    auto result = *this;
    result.size.transpose();
    for (auto &frame : result.frames)
        for (auto &layer : frame.layers) {
            const auto old = layer.image.size();
            const auto pos = layer.position;
            layer.image = rotate(layer.image, clockwise);
            layer.position = clockwise ? QPoint(size.height() - pos.y() - old.height(), pos.x())
                                       : QPoint(pos.y(), size.width() - pos.x() - old.width());
        }
    result.anchor = clockwise ? QPoint(size.height() - anchor.y(), anchor.x())
                              : QPoint(anchor.y(), size.width() - anchor.x());
    result.validate();
    return result;
}
SpriteLibrary SpriteLibrary::fromJson(const QJsonObject &object) {
    const int version = integer(object["schemaVersion"], 1, 3, "library version");
    if (!object["assets"].isArray() || object["assets"].toArray().size() > 512)
        throw Error("Invalid sprite asset count");
    SpriteLibrary result;
    for (const auto &value : object["assets"].toArray()) {
        const auto asset = SpriteAsset::fromJson(value.toObject(), version);
        for (const auto &existing : result.assets_)
            if (existing.id == asset.id)
                throw Error("Duplicate sprite asset ID");
        result.replace(asset);
    }
    return result;
}
QJsonObject SpriteLibrary::json(bool browser) const {
    QJsonArray objects;
    for (const auto &asset : assets_)
        objects.append(asset.json(browser));
    return {{"schemaVersion", browser ? 2 : 3}, {"assets", objects}};
}
const SpriteAsset &SpriteLibrary::asset(const QString &id) const {
    for (const auto &asset : assets_)
        if (asset.id == id)
            return asset;
    throw Error("Sprite asset no longer exists");
}
void SpriteLibrary::replace(const SpriteAsset &asset) {
    asset.validate();
    quint64 pixels = asset.pixels();
    int found = -1;
    for (int i = 0; i < assets_.size(); ++i)
        if (assets_[i].id == asset.id)
            found = i;
        else
            pixels += assets_[i].pixels();
    if (pixels > pixelLimit || (found < 0 && assets_.size() >= 512))
        throw Error("Sprite project budget exceeded");
    if (found < 0)
        assets_.append(asset);
    else
        assets_[found] = asset;
}
void SpriteLibrary::remove(const QString &id) {
    for (int i = 0; i < assets_.size(); ++i)
        if (assets_[i].id == id) {
            assets_.removeAt(i);
            return;
        }
    throw Error("Sprite asset no longer exists");
}
void SpriteLibrary::merge(const SpriteLibrary &other) {
    auto candidate = *this;
    for (const auto &asset : other.assets_)
        candidate.replace(asset);
    *this = std::move(candidate);
}
QImage spriteLine(QImage image, QPoint from, QPoint to, const QColor &color, int brush,
                  QRect selection) {
    if (image.format() != QImage::Format_RGBA8888 || brush < 1 || brush > 64)
        throw Error("Invalid pixel brush");
    for (auto point : {from, to})
        if (point.x() < -4096 || point.x() > 8192 || point.y() < -4096 || point.y() > 8192)
            throw Error("Pixel stroke exceeds canvas bounds");
    const auto bounds = (selection.isNull() ? image.rect() : selection.intersected(image.rect()));
    const auto pixel = pixelWord(color);
    int x = from.x(), y = from.y();
    const int dx = std::abs(to.x() - x), dy = -std::abs(to.y() - y), sx = x < to.x() ? 1 : -1,
              sy = y < to.y() ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        const auto stamp = QRect(x - brush / 2, y - brush / 2, brush, brush).intersected(bounds);
        if (!stamp.isEmpty())
            for (int row = stamp.top(); row <= stamp.bottom(); ++row)
                std::fill_n(reinterpret_cast<quint32 *>(image.scanLine(row)) + stamp.left(),
                            stamp.width(), pixel);
        if (x == to.x() && y == to.y())
            break;
        const int twice = error * 2;
        if (twice >= dy) {
            error += dy;
            x += sx;
        }
        if (twice <= dx) {
            error += dx;
            y += sy;
        }
    }
    return image;
}
QImage spriteFill(QImage image, QPoint start, const QColor &color, QRect selection) {
    if (image.format() != QImage::Format_RGBA8888)
        throw Error("Invalid flood-fill image");
    const auto bounds = selection.isNull() ? image.rect() : selection.intersected(image.rect());
    if (!bounds.contains(start))
        return image;
    const auto before = readPixel(image, start.x(), start.y()), after = pixelWord(color);
    if (before == after)
        return image;
    QVector<QPoint> stack{start};
    while (!stack.isEmpty()) {
        const auto point = stack.takeLast();
        const int y = point.y();
        if (readPixel(image, point.x(), y) != before)
            continue;
        int left = point.x(), right = point.x();
        while (left > bounds.left() && readPixel(image, left - 1, y) == before)
            --left;
        while (right < bounds.right() && readPixel(image, right + 1, y) == before)
            ++right;
        for (int x = left; x <= right; ++x)
            writePixel(image, x, y, after);
        for (int ny : {y - 1, y + 1})
            if (ny >= bounds.top() && ny <= bounds.bottom()) {
                bool span = false;
                for (int x = left; x <= right; ++x) {
                    const bool target = readPixel(image, x, ny) == before;
                    if (target && !span)
                        stack.append({x, ny});
                    span = target;
                }
            }
    }
    return image;
}
QImage spriteShift(const QImage &image, QPoint offset, bool wrap) {
    if (image.isNull() || image.format() != QImage::Format_RGBA8888)
        throw Error("Invalid sprite shift image");
    positionValid(offset);
    auto out = transparent(image.size());
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            int sx = x - offset.x(), sy = y - offset.y();
            if (wrap) {
                sx = ((sx % image.width()) + image.width()) % image.width();
                sy = ((sy % image.height()) + image.height()) % image.height();
            }
            if (image.rect().contains(sx, sy))
                writePixel(out, x, y, readPixel(image, sx, sy));
        }
    return out;
}
QImage spriteMoveSelection(const QImage &source, QRect rect, QPoint offset) {
    if (source.isNull() || source.format() != QImage::Format_RGBA8888)
        throw Error("Invalid sprite selection image");
    positionValid(offset);
    rect = rect.intersected(source.rect());
    if (rect.isEmpty() || offset.isNull())
        return source;
    auto out = source;
    for (int y = rect.top(); y <= rect.bottom(); ++y)
        std::memset(out.scanLine(y) + rect.x() * 4, 0, rect.width() * 4);
    const auto target = rect.translated(offset).intersected(source.rect());
    if (!target.isEmpty())
        for (int y = target.top(); y <= target.bottom(); ++y)
            std::memcpy(out.scanLine(y) + target.x() * 4,
                        source.constScanLine(y - offset.y()) + (target.x() - offset.x()) * 4,
                        target.width() * 4);
    return out;
}
} // namespace lordly
