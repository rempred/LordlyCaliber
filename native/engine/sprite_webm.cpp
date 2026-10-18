#include "engine/sprite_video.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <algorithm>

namespace lordly {
bool exportSpriteWebm(const SpriteAsset &asset, const QString &path,
                      const SpriteVideoOptions &options,
                      const std::function<bool(int, int)> &progress) {
    const auto plan = spriteVideoPlan(asset, options);
    const auto ffmpeg = QStandardPaths::findExecutable("ffmpeg");
    if (ffmpeg.isEmpty())
        throw Error("WebM export requires FFmpeg with the libvpx-vp9 encoder on PATH. "
                    "Install FFmpeg and restart the editor; PNG export is available without it.");
    QTemporaryDir temporary(QFileInfo(path).absoluteDir().filePath(".lordly-webm-XXXXXX"));
    if (!temporary.isValid())
        throw Error("Cannot create temporary WebM output: " + temporary.errorString());
    const auto intermediate = temporary.filePath("animation.webm");
    QProcess encoder;
    const auto stopEncoder = qScopeGuard([&] {
        if (encoder.state() != QProcess::NotRunning) {
            encoder.kill();
            encoder.waitForFinished(5000);
        }
    });
    encoder.setStandardOutputFile(QProcess::nullDevice());
    encoder.start(ffmpeg, {"-hide_banner",
                           "-loglevel",
                           "error",
                           "-nostdin",
                           "-y",
                           "-f",
                           "rawvideo",
                           "-pixel_format",
                           "rgba",
                           "-video_size",
                           QString("%1x%2").arg(plan.size.width()).arg(plan.size.height()),
                           "-framerate",
                           QString::number(options.framesPerSecond),
                           "-i",
                           "pipe:0",
                           "-an",
                           // Keep alpha in its original 8-bit plane. Direct RGBA
                           // to YUVA conversion can round some alpha values up.
                           "-vf",
                           "split[color][alpha];[color]format=yuv420p[c];[alpha]alphaextract[a];[c][a]alphamerge",
                           "-c:v",
                           "libvpx-vp9",
                           "-lossless",
                           "1",
                           "-pix_fmt",
                           "yuva420p",
                           "-auto-alt-ref",
                           "0",
                           "-deadline",
                           "good",
                           "-cpu-used",
                           "4",
                           "-threads",
                           "2",
                           "-f",
                           "webm",
                           intermediate});
    if (!encoder.waitForStarted(5000))
        throw Error("Cannot start FFmpeg: " + encoder.errorString());
    int completed = 0;
    QByteArray errors;
    const auto cancelled = [&] { return progress && !progress(completed, plan.ticks); };
    const auto collectErrors = [&] {
        errors = (errors + encoder.readAllStandardError()).right(4096);
    };
    const auto failed = [&] {
        collectErrors();
        throw Error("WebM encoding failed: " +
                    (errors.isEmpty() ? encoder.errorString() : QString::fromUtf8(errors)));
    };
    for (int repeat = 0; repeat < options.repeats; ++repeat) {
        for (int f = 0; f < asset.frames.size(); ++f) {
            if (cancelled())
                return false;
            const auto image = asset.composite(f)
                                   .scaled(plan.size, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                                   .convertToFormat(QImage::Format_RGBA8888);
            for (int hold = 0; hold < std::max(1, asset.frames[f].ticks); ++hold) {
                if (cancelled())
                    return false;
                if (encoder.state() == QProcess::NotRunning ||
                    encoder.write(reinterpret_cast<const char *>(image.constBits()),
                                  image.sizeInBytes()) != image.sizeInBytes())
                    failed();
                QElapsedTimer idle;
                idle.start();
                while (encoder.bytesToWrite()) {
                    if (cancelled())
                        return false;
                    if (encoder.waitForBytesWritten(50))
                        idle.restart();
                    collectErrors();
                    if (encoder.state() == QProcess::NotRunning)
                        failed();
                    if (idle.elapsed() > 30000)
                        throw Error("FFmpeg stopped accepting animation frames");
                }
                ++completed;
            }
        }
    }
    encoder.closeWriteChannel();
    QElapsedTimer finishing;
    finishing.start();
    while (encoder.state() != QProcess::NotRunning) {
        if (cancelled())
            return false;
        encoder.waitForFinished(50);
        collectErrors();
        if (finishing.elapsed() > 30000)
            throw Error("FFmpeg timed out finishing the WebM");
    }
    if (encoder.exitStatus() != QProcess::NormalExit || encoder.exitCode() != 0)
        failed();
    if (cancelled())
        return false;
    QFile encoded(intermediate);
    QSaveFile destination(path);
    if (!encoded.open(QIODevice::ReadOnly) || encoded.size() == 0 ||
        !destination.open(QIODevice::WriteOnly))
        throw Error("Cannot save the completed WebM");
    while (!encoded.atEnd()) {
        if (cancelled())
            return false;
        const auto bytes = encoded.read(1024 * 1024);
        if (bytes.isEmpty() || destination.write(bytes) != bytes.size())
            throw Error("Cannot write the completed WebM");
    }
    if (cancelled())
        return false;
    if (!destination.commit())
        throw Error("Cannot commit WebM: " + destination.errorString());
    return true;
}
} // namespace lordly
