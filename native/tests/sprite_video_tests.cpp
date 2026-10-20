#include "engine/sprite_video.h"
#include "engine/animation_file_import.h"
#include "engine/project.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
namespace {
QByteArray run(const QString &program, const QStringList &args) {
    QProcess process;
    process.start(program, args);
    if (!process.waitForFinished(30000) || process.exitCode() != 0)
        throw Error("Video verification failed: " +
                    QString::fromUtf8(process.readAllStandardError()));
    return process.readAllStandardOutput();
}
SpriteAsset example() {
    auto asset = SpriteAsset::blank("Video timing", {64, 64});
    asset.kind = "sequence";
    const QColor top[]{Qt::red, Qt::blue, Qt::cyan}, bottom[]{Qt::green, Qt::yellow, Qt::magenta};
    for (int f = 0; f < 3; ++f) {
        auto frame = asset.frames.first();
        frame.id = SpriteLibrary::newId();
        frame.ticks = f ? f + 1 : 0;
        auto &image = frame.layers.first().image;
        image.fill(Qt::transparent);
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 48; ++x)
                image.setPixelColor(x, y, y < 32 ? top[f] : bottom[f]);
        if (!f)
            asset.frames.clear();
        asset.frames << frame;
    }
    return asset;
}
} // namespace
class SpriteVideoTests : public QObject {
    Q_OBJECT
  private slots:
    void frameZipImport() {
        QTemporaryDir work;
        auto original = example();
        original.anchor = {13, 41};
        const auto path = work.filePath("frames.zip");
        QVERIFY(exportSpriteFramesZip(original, path, 15));
        const auto imported = importAnimationFiles({path});
        QCOMPARE(imported.size, original.size);
        QCOMPARE(imported.anchor, original.anchor);
        QCOMPARE(imported.frames.size(), original.frames.size());
        for (int i = 0; i < original.frames.size(); ++i) {
            QCOMPARE(imported.composite(i), original.composite(i));
            QCOMPARE(imported.frames[i].ticks, std::max(1, original.frames[i].ticks) * 2);
        }
        // Readback also accepts a normally deflated ZIP, not only our stored writer.
        const auto python = QStandardPaths::findExecutable("python");
        if (!python.isEmpty()) {
            const auto packed = work.filePath("compressed.zip");
            run(python, {"-c",
                         "import zipfile,sys\na=zipfile.ZipFile(sys.argv[1]); "
                         "b=zipfile.ZipFile(sys.argv[2],'w',zipfile.ZIP_DEFLATED)\nfor n in "
                         "a.namelist(): b.writestr(n,a.read(n))\nb.close()",
                         path, packed});
            QCOMPARE(importAnimationFiles({packed}).composite(2), original.composite(2));
        }
        QFile zip(path);
        QVERIFY(zip.open(QIODevice::ReadOnly));
        auto bytes = zip.readAll();
        zip.close();
        const int payload = bytes.indexOf(QByteArray::fromHex("89504e470d0a1a0a"));
        QVERIFY(payload > 0);
        bytes[payload + 20] ^= 1;
        const auto corrupt = work.filePath("corrupt.zip");
        atomicWrite(corrupt, bytes);
        QVERIFY_EXCEPTION_THROWN(importAnimationFiles({corrupt}), Error);
        atomicWrite(corrupt, bytes.left(17));
        QVERIFY_EXCEPTION_THROWN(importAnimationFiles({corrupt}), Error);
        // Natural order, common canvas and explicit PNG playback speed.
        const auto first = work.filePath("frame-2.png"), second = work.filePath("frame-10.png");
        QVERIFY(original.composite(0).save(first));
        QVERIFY(original.composite(1).save(second));
        const auto pngs = importAnimationFiles({second, first}, 10);
        QCOMPARE(pngs.composite(0), original.composite(0));
        QCOMPARE(pngs.composite(1), original.composite(1));
        QCOMPARE(pngs.frames[0].ticks, 3);
        QCOMPARE(pngs.anchor, QPoint(32, 64));
        auto wrong = QImage(3, 4, QImage::Format_RGBA8888);
        wrong.fill(Qt::red);
        QVERIFY(wrong.save(second));
        QVERIFY_EXCEPTION_THROWN(importAnimationFiles({first, second}), Error);
        // A long hold in our own export must not count each repeated PNG as
        // another resident image (80 million decoded pixels, one 1-Mpixel image).
        auto held = SpriteAsset::blank("Long hold", {1024, 1024});
        held.frames[0].ticks = 80;
        held.frames[0].layers[0].image.fill(Qt::red);
        QVERIFY(exportSpriteFramesZip(held, path, 30));
        const auto heldImport = importAnimationFiles({path});
        QCOMPARE(heldImport.frames.size(), 1);
        QCOMPARE(heldImport.frames[0].ticks, 80);
    }
    void webmSequence() {
        const auto ffmpeg = QStandardPaths::findExecutable("ffmpeg"),
                   probe = QStandardPaths::findExecutable("ffprobe");
        if (ffmpeg.isEmpty() || probe.isEmpty())
            QSKIP("FFmpeg and FFprobe required");
        try {
            auto asset = example();
            asset.frames[1].layers[0].image.fill(QColor(40, 80, 120, 73));
            const SpriteVideoOptions options{1, 2, Qt::black, SpriteVideoFormat::Webm};
            QTemporaryDir work;
            const auto path = work.filePath("transparent sequence.webm");
            int latest = 0;
            QVERIFY(exportSpriteVideo(asset, path, options, [&](int done, int total) {
                if (done < latest || total != 12)
                    throw Error("Invalid WebM progress");
                latest = done;
                return true;
            }));
            QCOMPARE(latest, 12);
            const auto metadata =
                QJsonDocument::fromJson(run(probe, {"-v", "error", "-show_streams", "-show_frames",
                                                    "-of", "json", path}))
                    .object();
            const auto stream = metadata["streams"].toArray().first().toObject();
            QCOMPARE(stream["codec_name"].toString(), QString("vp9"));
            QCOMPARE(stream["width"].toInt(), 64);
            QCOMPARE(stream["height"].toInt(), 64);
            QCOMPARE(metadata["frames"].toArray().size(), 12);
            for (int i = 0; i < 12; ++i)
                QVERIFY(std::abs(metadata["frames"]
                                     .toArray()[i]
                                     .toObject()["best_effort_timestamp_time"]
                                     .toString()
                                     .toDouble() -
                                 i / 30.) < .0011);
            // FFmpeg's native VP9 decoder ignores alpha; libvpx decodes the WebM alpha plane.
            const auto decoded = run(ffmpeg, {"-v", "error", "-c:v", "libvpx-vp9", "-i", path, "-f",
                                              "rawvideo", "-pix_fmt", "rgba", "-"});
            QCOMPARE(decoded.size(), 12 * 64 * 64 * 4);
            const int sourceFrames[]{0, 1, 1, 2, 2, 2};
            for (int i = 0; i < 12; ++i) {
                const auto expected = asset.composite(sourceFrames[i % 6]);
                const auto *bytes =
                    reinterpret_cast<const uchar *>(decoded.constData()) + i * 64 * 64 * 4;
                for (int p = 0; p < 64 * 64; ++p)
                    QCOMPARE(bytes[p * 4 + 3], expected.constBits()[p * 4 + 3]);
                for (int c = 0; c < 3; ++c)
                    QVERIFY(std::abs(int(bytes[(10 * 64 + 10) * 4 + c]) -
                                     int(expected.constBits()[(10 * 64 + 10) * 4 + c])) <= 4);
            }
            QFile saved(path);
            QVERIFY(saved.open(QIODevice::ReadOnly));
            const auto original = saved.readAll();
            saved.close();
            QVERIFY(
                !exportSpriteVideo(asset, path, options, [](int done, int) { return done < 2; }));
            QVERIFY(saved.open(QIODevice::ReadOnly));
            QCOMPARE(saved.readAll(), original);
            saved.close();
            // A missing encoder reports a failure without replacing a prior export.
            const auto priorPath = qgetenv("PATH");
            qputenv("PATH", work.path().toUtf8());
            bool failed = false;
            try {
                exportSpriteVideo(asset, path, options);
            } catch (const Error &) {
                failed = true;
            }
            qputenv("PATH", priorPath);
            QVERIFY(failed);
            QVERIFY(saved.open(QIODevice::ReadOnly));
            QCOMPARE(saved.readAll(), original);
            auto tiny = SpriteAsset::blank("Odd dimensions", {3, 5});
            tiny.frames[0].layers[0].image.fill(QColor(40, 80, 120, 128));
            const SpriteVideoOptions scaled{3, 1, Qt::black, SpriteVideoFormat::Webm};
            QVERIFY(exportSpriteVideo(tiny, work.filePath("odd.webm"), scaled));
            const auto odd =
                QJsonDocument::fromJson(run(probe, {"-v", "error", "-show_streams", "-of", "json",
                                                    work.filePath("odd.webm")}))
                    .object()["streams"]
                    .toArray()
                    .first()
                    .toObject();
            QCOMPARE(odd["width"].toInt(), 9);
            QCOMPARE(odd["height"].toInt(), 15);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void transparentAnimation() {
        const auto ffmpeg = QStandardPaths::findExecutable("ffmpeg"), probe = QStandardPaths::findExecutable("ffprobe");
        if (ffmpeg.isEmpty() || probe.isEmpty()) QSKIP("Independent FFmpeg decoder required for this development check");
        try {
            auto asset = example();
            asset.frames[0].layers[0].image.setPixelColor(0, 0, QColor(120, 40, 20, 128));
            asset.frames[1].layers[0].image.fill(Qt::transparent);
            asset.frames[1].layers[0].image.setPixelColor(3, 3, QColor(9, 70, 33, 73));
            QTemporaryDir work; const auto path = work.filePath("transparent.apng");
            const SpriteVideoOptions options{1, 0, Qt::black, SpriteVideoFormat::AnimatedPng};
            QVERIFY(exportSpriteVideo(asset, path, options));
            const auto decoded = run(ffmpeg, {"-v", "error", "-ignore_loop", "1", "-i", path,
                                             "-fps_mode", "passthrough", "-f", "rawvideo", "-pix_fmt", "rgba", "-"});
            QByteArray expected;
            for (int f = 0; f < 3; ++f) {
                const auto image = asset.composite(f);
                expected.append(reinterpret_cast<const char *>(image.constBits()), image.sizeInBytes());
            }
            QCOMPARE(decoded, expected);
            const auto metadata = QJsonDocument::fromJson(run(probe, {"-v", "error", "-ignore_loop", "1",
                                                                     "-show_frames", "-of", "json", path})).object();
            const auto frames = metadata["frames"].toArray(); QCOMPARE(frames.size(), 3);
            for (int f = 0; f < 3; ++f)
                QVERIFY(std::abs(frames[f].toObject()["duration_time"].toString().toDouble() - (f + 1) / 30.) < .00001);
            QFile encoded(path); QVERIFY(encoded.open(QIODevice::ReadOnly)); const auto bytes = encoded.readAll(); encoded.close();
            // Loop count is container metadata, independent of decoder playback settings.
            const int control = bytes.indexOf("acTL"); QVERIFY(control >= 4);
            QCOMPARE(u32(bytes, control + 4), quint32(3)); QCOMPARE(u32(bytes, control + 8), quint32(0));
            QVERIFY(!exportSpriteVideo(asset, path, options, [](int done, int) { return done < 1; }));
            QVERIFY(encoded.open(QIODevice::ReadOnly)); QCOMPARE(encoded.readAll(), bytes); encoded.close();
            auto tiny = SpriteAsset::blank("Tiny alpha", {3, 5}); tiny.frames[0].layers[0].image.fill(QColor(20, 40, 60, 128));
            const SpriteVideoOptions scaled{2, 3, Qt::black, SpriteVideoFormat::AnimatedPng};
            QCOMPARE(spriteVideoPlan(tiny, scaled).size, QSize(6, 10));
            QVERIFY(exportSpriteVideo(tiny, work.filePath("tiny.apng"), scaled));
            QFile tinyFile(work.filePath("tiny.apng")); QVERIFY(tinyFile.open(QIODevice::ReadOnly));
            const auto tinyBytes = tinyFile.readAll(); QCOMPARE(u32(tinyBytes, tinyBytes.indexOf("acTL") + 8), quint32(3));
            const auto tinyPixels = run(ffmpeg, {"-v", "error", "-ignore_loop", "1", "-i", work.filePath("tiny.apng"),
                                                "-frames:v", "1", "-f", "rawvideo", "-pix_fmt", "rgba", "-"});
            const auto tinyExpected = tiny.composite(0).scaled(6, 10, Qt::IgnoreAspectRatio, Qt::FastTransformation);
            QCOMPARE(tinyPixels, QByteArray(reinterpret_cast<const char *>(tinyExpected.constBits()), tinyExpected.sizeInBytes()));
        } catch (const Error &e) { QFAIL(e.what()); }
    }
    void encodeAndDecode() {
        const auto ffmpeg = QStandardPaths::findExecutable("ffmpeg"),
                   probe = QStandardPaths::findExecutable("ffprobe");
        if (ffmpeg.isEmpty() || probe.isEmpty())
            QSKIP("Independent FFmpeg decoder required for this development check");
        try {
            QTemporaryDir work;
            const auto path = work.filePath("timing.mp4");
            const auto asset = example();
            const SpriteVideoOptions options{1, 2, QColor(32, 32, 32)};
            int latest = 0;
            QVERIFY(exportSpriteVideo(asset, path, options, [&](int done, int total) {
                if (done < latest || total != 12)
                    throw Error("Invalid export progress");
                latest = done;
                return true;
            }));
            QCOMPARE(latest, 12);
            const auto metadata =
                QJsonDocument::fromJson(
                    run(probe, {"-v", "error", "-show_streams", "-of", "json", path}))
                    .object();
            const auto stream = metadata["streams"].toArray().first().toObject();
            QCOMPARE(stream["codec_name"].toString(), QString("h264"));
            QCOMPARE(stream["width"].toInt(), 64);
            QCOMPARE(stream["height"].toInt(), 64);
            QCOMPARE(stream["nb_frames"].toString().toInt(), 12);
            QVERIFY(std::abs(stream["duration"].toString().toDouble() - .4) < .0001);
            const auto decoded =
                run(ffmpeg, {"-v", "error", "-i", path, "-f", "rawvideo", "-pix_fmt", "rgba", "-"});
            QCOMPARE(decoded.size(), 12 * 64 * 64 * 4);
            const int order[]{0, 1, 1, 2, 2, 2};
            for (int f = 0; f < 12; ++f)
                for (QPoint point : {QPoint(16, 8), QPoint(16, 56), QPoint(56, 32)}) {
                    const auto expected = point.x() >= 48
                                              ? options.background
                                              : asset.composite(order[f % 6]).pixelColor(point);
                    const int at = ((f * 64 + point.y()) * 64 + point.x()) * 4;
                    const QColor actual(u8(decoded, at), u8(decoded, at + 1), u8(decoded, at + 2));
                    QVERIFY2(std::abs(actual.red() - expected.red()) <= 20 &&
                                 std::abs(actual.green() - expected.green()) <= 20 &&
                                 std::abs(actual.blue() - expected.blue()) <= 20,
                             qPrintable(QString("Frame %1 at %2,%3: %4 expected %5")
                                            .arg(f)
                                            .arg(point.x())
                                            .arg(point.y())
                                            .arg(actual.name(), expected.name())));
                }
            auto odd = SpriteAsset::blank("Odd size", {17, 19});
            odd.frames[0].ticks = 1;
            odd.frames[0].layers[0].image.fill(Qt::white);
            QCOMPARE(spriteVideoPlan(odd, {1, 1, Qt::black}).size, QSize(64, 64));
            const auto oddPath = work.filePath("odd.mp4");
            QVERIFY(exportSpriteVideo(odd, oddPath, {1, 1, Qt::black}));
            const auto padded = run(
                ffmpeg, {"-v", "error", "-i", oddPath, "-f", "rawvideo", "-pix_fmt", "rgba", "-"});
            QCOMPARE(padded.size(), 64 * 64 * 4);
            QVERIFY(u8(padded, (10 * 64 + 10) * 4) > 235);
            QVERIFY(u8(padded, (24 * 64 + 24) * 4) < 20);
            qInfo() << "Independent decode: 12 frames, 0.4 seconds, 36 color/orientation samples; "
                       "odd-size export passed";
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void cancellationPreservesDestination() {
        try {
            QTemporaryDir work;
            const auto path = work.filePath("existing.mp4");
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("existing video");
            file.close();
            QVERIFY(!exportSpriteVideo(example(), path, {1, 1, Qt::black},
                                       [](int done, int) { return done < 2; }));
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), QByteArray("existing video"));
            file.close();
            QCOMPARE(
                QDir(work.path()).entryList({".lordly-video-*"}, QDir::Dirs | QDir::Hidden).size(),
                0);
            QVERIFY_THROWS_EXCEPTION(Error, exportSpriteVideo(example(), path, {0, 1, Qt::black}));
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), QByteArray("existing video"));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(SpriteVideoTests)
#include "sprite_video_tests.moc"
