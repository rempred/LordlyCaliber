#include "engine/sprite_video.h"
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
