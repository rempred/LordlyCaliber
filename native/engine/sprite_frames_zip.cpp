#include "engine/sprite_video.h"
#include <QBuffer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QtZlib/zlib.h>

namespace lordly {
namespace {
void le16(QByteArray &out, quint16 v) { out.append(char(v)); out.append(char(v >> 8)); }
void le32(QByteArray &out, quint32 v) { le16(out, quint16(v)); le16(out, quint16(v >> 16)); }
void write(QSaveFile &out, const QByteArray &bytes) {
    if (out.pos() + bytes.size() > 512LL * 1024 * 1024)
        throw Error("Frame ZIP exceeds 512 MiB");
    if (out.write(bytes) != bytes.size()) throw Error("Cannot write frame ZIP: " + out.errorString());
}
// ZIP32 stored members: PNG already compresses its image data. Fixed ASCII
// names, CRC32 and a central directory, per PKWARE APPNOTE sections 4.3.7–4.3.16.
void member(QSaveFile &out, QByteArray &directory, const QByteArray &name, const QByteArray &data) {
    const auto offset = quint32(out.pos());
    const auto size = quint32(data.size());
    const auto crc = quint32(crc32(0, reinterpret_cast<const Bytef *>(data.constData()), uInt(data.size())));
    QByteArray header;
    le32(header, 0x04034b50); le16(header, 20); le16(header, 0); le16(header, 0);
    le16(header, 0); le16(header, 0x21); // 1980-01-01: deterministic DOS timestamp.
    le32(header, crc); le32(header, size); le32(header, size);
    le16(header, quint16(name.size())); le16(header, 0);
    write(out, header + name); write(out, data);
    le32(directory, 0x02014b50); le16(directory, 20);
    directory += header.mid(4);
    le16(directory, 0); le16(directory, 0); le16(directory, 0); le32(directory, 0);
    le32(directory, offset); directory += name;
}
} // namespace
bool exportSpriteFramesZip(const SpriteAsset &asset, const QString &path, int fps,
                           const std::function<bool(int, int)> &progress) {
    const auto plan = spriteVideoPlan(asset, {1, 1, Qt::transparent, SpriteVideoFormat::AnimatedPng, fps});
    if (path.isEmpty()) throw Error("Choose a frame ZIP destination");
    if (plan.ticks > 65532) throw Error("Frame ZIP exceeds the ZIP32 member limit");
    if (progress && !progress(0, plan.ticks)) return false;
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) throw Error("Cannot create frame ZIP: " + out.errorString());
    QByteArray directory;
    QJsonArray frames;
    int tick = 0;
    for (int i = 0; i < asset.frames.size(); ++i) {
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        if (!asset.composite(i).save(&buffer, "PNG")) throw Error("Cannot encode animation frame");
        for (int hold = 0; hold < std::max(1, asset.frames[i].ticks); ++hold) {
            if (progress && !progress(tick, plan.ticks)) return false;
            const auto name = QString("frame-%1.png").arg(tick + 1, 5, 10, QChar('0'));
            member(out, directory, name.toUtf8(), png);
            frames << QJsonObject{{"file", name}, {"startTick", tick}, {"durationTicks", 1}};
            ++tick;
        }
    }
    const QJsonObject manifest{{"format", "lordly-frame-sequence"}, {"version", 1},
        {"framesPerSecond", fps}, {"totalTicks", tick},
        {"width", asset.size.width()}, {"height", asset.size.height()},
        {"origin", QJsonObject{{"x", asset.anchor.x()}, {"y", asset.anchor.y()}}},
        {"frames", frames}, {"source", asset.provenance}};
    member(out, directory, "animation.json", QJsonDocument(manifest).toJson());
    member(out, directory, "README.txt",
        QString("Import frame-*.png as an image sequence at %1 frames per second.\n"
                "Each PNG is one playback step; repeated images preserve holds.\n"
                "All images share a canvas and origin (see animation.json).\n"
                "Transparent artwork only: no editor guides, onion skins or sound.\n").arg(fps).toUtf8());
    const auto start = quint32(out.pos());
    write(out, directory);
    QByteArray end;
    le32(end, 0x06054b50); le16(end, 0); le16(end, 0);
    le16(end, quint16(tick + 2)); le16(end, quint16(tick + 2));
    le32(end, quint32(directory.size())); le32(end, start); le16(end, 0);
    write(out, end);
    if (progress && !progress(tick, plan.ticks)) return false;
    if (!out.commit()) throw Error("Cannot commit frame ZIP: " + out.errorString());
    return true;
}
} // namespace lordly
