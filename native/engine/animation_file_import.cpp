#include "engine/animation_file_import.h"
#include "engine/image_colors.h"
#include <QBuffer>
#include <QCollator>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QtZlib/zlib.h>
#include <algorithm>
#include <cmath>

namespace lordly {
namespace {
constexpr qint64 byteLimit = 512LL * 1024 * 1024;
QByteArray read(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > byteLimit)
        throw Error("Cannot read animation file, or it exceeds 512 MiB: " + path);
    return file.readAll();
}
int number(const QJsonValue &v, int lo, int hi) {
    const double n = v.toDouble(NAN);
    if (!std::isfinite(n) || n != std::floor(n) || n < lo || n > hi)
        throw Error("Invalid frame-sequence metadata");
    return int(n);
}
quint16 le16(const QByteArray &b, qsizetype at) {
    return u8(b, at) | (quint16(u8(b, at + 1)) << 8);
}
quint32 le32(const QByteArray &b, qsizetype at) {
    return le16(b, at) | (quint32(le16(b, at + 2)) << 16);
}
struct Member {
    quint32 crc, packed, size, offset;
    quint16 method, flags;
};
class Zip {
    QByteArray bytes;
    QMap<QString, Member> members;
    quint32 directory;

  public:
    explicit Zip(QByteArray data) : bytes(std::move(data)) {
        qsizetype end = bytes.size() - 22;
        for (; end >= std::max<qsizetype>(0, bytes.size() - 65557); --end)
            if (le32(bytes, end) == 0x06054b50 && end + 22 + le16(bytes, end + 20) == bytes.size())
                break;
        if (end < 0 || end < bytes.size() - 65557 || le16(bytes, end + 4) || le16(bytes, end + 6) ||
            le16(bytes, end + 8) != le16(bytes, end + 10))
            throw Error("Unsupported or damaged animation ZIP");
        const int count = le16(bytes, end + 10);
        directory = le32(bytes, end + 16);
        if (!count || count == 65535 || quint64(directory) + le32(bytes, end + 12) != quint64(end))
            throw Error("Invalid ZIP directory");
        qsizetype cursor = directory;
        quint64 total = 0;
        for (int i = 0; i < count; ++i) {
            if (cursor + 46 > end || le32(bytes, cursor) != 0x02014b50)
                throw Error("Invalid ZIP member");
            const int nameSize = le16(bytes, cursor + 28), extra = le16(bytes, cursor + 30),
                      comment = le16(bytes, cursor + 32);
            if (cursor + 46 + nameSize + extra + comment > end)
                throw Error("Truncated ZIP member");
            const auto name = QString::fromUtf8(bytes.mid(cursor + 46, nameSize));
            Member m{le32(bytes, cursor + 16), le32(bytes, cursor + 20), le32(bytes, cursor + 24),
                     le32(bytes, cursor + 42), le16(bytes, cursor + 10), le16(bytes, cursor + 8)};
            total += m.size;
            if (name.isEmpty() || name.contains('\\') || name.startsWith('/') ||
                name.contains(':') || name.split('/').contains("..") || name.contains(QChar(0)) ||
                members.contains(name) || total > byteLimit || m.size > byteLimit ||
                m.packed > byteLimit || (m.flags & ~quint16(0x808)) ||
                (m.method != 0 && m.method != 8) || le16(bytes, cursor + 34))
                throw Error("Unsupported or unsafe animation ZIP member");
            members[name] = m;
            cursor += 46 + nameSize + extra + comment;
        }
        if (cursor != end)
            throw Error("ZIP directory length differs from its members");
    }
    QByteArray member(const QString &name, qint64 limit) const {
        if (!members.contains(name))
            throw Error("Animation ZIP is missing " + name);
        const auto m = members[name];
        const qsizetype at = m.offset;
        if (m.size > limit || at + 30 > directory || le32(bytes, at) != 0x04034b50 ||
            le16(bytes, at + 6) != m.flags || le16(bytes, at + 8) != m.method)
            throw Error("Invalid ZIP member header");
        const int names = le16(bytes, at + 26), extra = le16(bytes, at + 28);
        const quint64 start = quint64(at) + 30 + names + extra;
        if (start + m.packed > directory || bytes.mid(at + 30, names) != name.toUtf8() ||
            (!(m.flags & 8) && (le32(bytes, at + 14) != m.crc || le32(bytes, at + 18) != m.packed ||
                                le32(bytes, at + 22) != m.size)))
            throw Error("ZIP member differs from its directory");
        QByteArray result;
        if (!m.method) {
            if (m.packed != m.size)
                throw Error("Invalid stored ZIP size");
            result = bytes.mid(qsizetype(start), m.size);
        } else {
            result.resize(m.size);
            z_stream stream{};
            stream.next_in =
                reinterpret_cast<Bytef *>(const_cast<char *>(bytes.constData() + start));
            stream.avail_in = m.packed;
            stream.next_out = reinterpret_cast<Bytef *>(result.data());
            stream.avail_out = m.size;
            if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
                throw Error("Cannot decode ZIP member");
            const int status = inflate(&stream, Z_FINISH);
            const bool valid =
                status == Z_STREAM_END && stream.total_in == m.packed && stream.total_out == m.size;
            inflateEnd(&stream);
            if (!valid)
                throw Error("Damaged compressed ZIP member");
        }
        if (quint32(crc32(0, reinterpret_cast<const Bytef *>(result.constData()),
                          uInt(result.size()))) != m.crc)
            throw Error("Animation ZIP checksum failed: " + name);
        return result;
    }
};
QImage png(QByteArray bytes, quint64 &pixels, QSize required = {}) {
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, "PNG");
    const auto size = reader.size();
    if (size.isEmpty() || size.width() > 4096 || size.height() > 4096 ||
        (!required.isEmpty() && size != required))
        throw Error("Animation frames must fit a common canvas of at most 4096 × 4096");
    pixels += quint64(size.width()) * size.height();
    if (pixels > SpriteLibrary::pixelLimit)
        throw Error("Animation exceeds the 64-million-pixel import budget");
    auto image = reader.read();
    if (image.isNull())
        throw Error("Cannot decode animation PNG");
    return imageToSrgb(image);
}
void append(SpriteAsset &asset, const QImage &image, int steps) {
    while (steps > 0) {
        if (!asset.frames.isEmpty() && asset.frames.last().ticks < 127 &&
            asset.frames.last().layers[0].image == image) {
            const int n = std::min(steps, 127 - asset.frames.last().ticks);
            asset.frames.last().ticks += n;
            steps -= n;
        } else {
            if (asset.frames.size() >= 1024)
                throw Error("Animation exceeds 1024 distinct frame holds");
            const int n = std::min(steps, 127);
            asset.frames << SpriteAssetFrame{
                SpriteLibrary::newId(),
                QString("Frame %1").arg(asset.frames.size() + 1),
                n,
                {{SpriteLibrary::newId(), "Artwork", true, {}, image}}};
            steps -= n;
        }
    }
}
} // namespace
SpriteAsset importAnimationFiles(QStringList paths, int fps) {
    if (paths.isEmpty() || fps < 1 || fps > 120)
        throw Error("Choose animation files and a valid frame rate");
    const auto suffix = QFileInfo(paths.first()).suffix().toLower();
    if (paths.size() == 1 && (suffix == "json" || suffix == "lcsprite"))
        return SpriteAsset::fromAssetFile(QJsonDocument::fromJson(read(paths.first())).object());
    SpriteAsset result;
    result.id = SpriteLibrary::newId();
    result.name = QFileInfo(paths.first()).completeBaseName().left(120);
    result.kind = "sequence";
    quint64 pixels = 0;
    if (paths.size() == 1 && suffix == "zip") {
        Zip zip(read(paths.first()));
        const auto meta =
            QJsonDocument::fromJson(zip.member("animation.json", 16 * 1024 * 1024)).object();
        if (meta["format"] != "lordly-frame-sequence" || number(meta["version"], 1, 1) != 1)
            throw Error("Choose a frame-sequence ZIP exported by Lordly Caliber");
        fps = number(meta["framesPerSecond"], 1, 120);
        const int total = number(meta["totalTicks"], 1, 65532);
        result.size = {number(meta["width"], 1, 4096), number(meta["height"], 1, 4096)};
        const auto origin = meta["origin"].toObject();
        result.anchor = {number(origin["x"], -32768, 32767), number(origin["y"], -32768, 32767)};
        const auto frames = meta["frames"].toArray();
        if (frames.isEmpty() || frames.size() > 65532)
            throw Error("Invalid animation frame list");
        int tick = 0;
        QSet<QString> files;
        QByteArray previousPng;
        QImage previousImage;
        for (const auto &value : frames) {
            const auto frame = value.toObject();
            const auto name = frame["file"].toString();
            if (!name.endsWith(".png", Qt::CaseInsensitive) || files.contains(name) ||
                number(frame["startTick"], 0, total) != tick)
                throw Error("Invalid animation frame order");
            files.insert(name);
            const int duration = number(frame["durationTicks"], 1, total);
            if (tick + duration > total)
                throw Error("Animation duration exceeds its declared total");
            const auto encoded = zip.member(name, 128 * 1024 * 1024);
            // Export repeats the same PNG to represent holds. Decode it once;
            // repeated time must not multiply the resident image budget.
            const auto image =
                encoded == previousPng ? previousImage : png(encoded, pixels, result.size);
            previousPng = encoded;
            previousImage = image;
            const int before = qRound(double(tick) * 30 / fps);
            tick += duration;
            append(result, image, qRound(double(tick) * 30 / fps) - before);
        }
        if (tick != total)
            throw Error("Animation duration differs from its frame list");
        result.provenance = meta["source"].toObject();
        result.provenance["importedFramesPerSecond"] = fps;
    } else {
        if (paths.size() > 1024)
            throw Error("Choose at most 1024 PNG frames");
        QCollator collator;
        collator.setNumericMode(true);
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        std::stable_sort(paths.begin(), paths.end(), [&](const auto &a, const auto &b) {
            return collator.compare(QFileInfo(a).fileName(), QFileInfo(b).fileName()) < 0;
        });
        for (int i = 0; i < paths.size(); ++i) {
            const auto image = png(read(paths[i]), pixels, result.size);
            if (result.size.isEmpty()) {
                result.size = image.size();
                result.anchor = {image.width() / 2, image.height()};
            }
            append(result, image, qRound(double(i + 1) * 30 / fps) - qRound(double(i) * 30 / fps));
        }
    }
    result.provenance["timing"] = "30-hz-preview-steps";
    result.validate();
    return result;
}
} // namespace lordly
