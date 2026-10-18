#include "engine/sprite_video.h"
#include <QBuffer>
#include <QSaveFile>
#include <QtZlib/zlib.h>
#include <algorithm>
namespace lordly {
namespace {
void write(QSaveFile &file, const QByteArray &bytes) {
    if (file.write(bytes) != bytes.size())
        throw Error("Cannot write animated PNG: " + file.errorString());
}
void chunk(QSaveFile &file, const QByteArray &type, const QByteArray &data) {
    QByteArray header(4, 0), trailer(4, 0);
    put32(header, 0, quint32(data.size()));
    auto crc = crc32(0, reinterpret_cast<const Bytef *>(type.constData()), 4);
    crc = crc32(crc, reinterpret_cast<const Bytef *>(data.constData()), uInt(data.size()));
    put32(trailer, 0, quint32(crc));
    write(file, header + type);
    write(file, data);
    write(file, trailer);
}
} // namespace
bool exportSpriteAnimatedPng(const SpriteAsset &asset, const QString &path,
                             const SpriteVideoOptions &options,
                             const std::function<bool(int, int)> &progress) {
    const auto plan = spriteVideoPlan(asset, options);
    if (options.format != SpriteVideoFormat::AnimatedPng || path.isEmpty())
        throw Error("Choose animated PNG and an output destination");
    if (progress && !progress(0, plan.ticks))
        return false;
    QSaveFile destination(path);
    if (!destination.open(QIODevice::WriteOnly))
        throw Error("Cannot create animated PNG: " + destination.errorString());
    write(destination, QByteArray::fromHex("89504e470d0a1a0a"));
    QByteArray header;
    quint32 sequence = 0;
    int completed = 0;
    for (int frame = 0; frame < asset.frames.size(); ++frame) {
        if (progress && !progress(completed, plan.ticks))
            return false;
        const auto image =
            asset.composite(frame).scaled(plan.size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "PNG"))
            throw Error("Cannot encode animation frame");
        QVector<QByteArray> data;
        QByteArray currentHeader;
        for (qsizetype at = 8; at + 12 <= png.size();) {
            const auto length = u32(png, at);
            if (quint64(at) + length + 12 > quint64(png.size()))
                throw Error("Invalid encoded PNG frame");
            const auto type = png.mid(at + 4, 4);
            if (type == "IHDR")
                currentHeader = png.mid(at + 8, length);
            if (type == "IDAT")
                data << png.mid(at + 8, length);
            at += length + 12;
        }
        if (currentHeader.size() != 13 || u8(currentHeader, 8) != 8 || u8(currentHeader, 9) != 6 ||
            data.isEmpty())
            throw Error("Animated PNG requires 8-bit RGBA frames");
        if (!frame) {
            header = currentHeader;
            chunk(destination, "IHDR", header);
            chunk(destination, "sRGB", QByteArray(1, 0));
            QByteArray animation(8, 0);
            put32(animation, 0, quint32(asset.frames.size()));
            put32(animation, 4, quint32(options.repeats));
            chunk(destination, "acTL", animation);
        } else if (currentHeader != header)
            throw Error("Animation frame formats differ");
        QByteArray control(26, 0);
        put32(control, 0, sequence++);
        put32(control, 4, quint32(plan.size.width()));
        put32(control, 8, quint32(plan.size.height()));
        const int ticks = std::max(1, asset.frames[frame].ticks);
        put16(control, 20, quint16(ticks));
        put16(control, 22, quint16(options.framesPerSecond));
        // Each complete RGBA frame replaces the canvas, avoiding alpha trails.
        chunk(destination, "fcTL", control);
        for (const auto &payload : data) {
            if (progress && !progress(completed, plan.ticks))
                return false;
            if (!frame)
                chunk(destination, "IDAT", payload);
            else {
                QByteArray prefix(4, 0);
                put32(prefix, 0, sequence++);
                chunk(destination, "fdAT", prefix + payload);
            }
        }
        completed += ticks * std::max(1, options.repeats);
    }
    chunk(destination, "IEND", {});
    if (progress && !progress(completed, plan.ticks))
        return false;
    if (!destination.commit())
        throw Error("Cannot commit animated PNG: " + destination.errorString());
    return true;
}
} // namespace lordly
