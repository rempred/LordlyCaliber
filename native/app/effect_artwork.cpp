#include "app/effect_artwork.h"
#include "engine/attack_data.h"
#include "engine/effect_resources.h"
#include "engine/sprite_video.h"
#include <QFutureWatcher>
#include <QPainter>
#include <QProgressDialog>
#include <QTimer>
#include <QtConcurrent>
#include <algorithm>
extern "C" unsigned int lc_effect_color(unsigned int, int, int, unsigned int);
namespace lordly {
bool effectTargetsAllies(const Attack &attack) {
    const int main = u8(attack.data, 7), spell = EffectPlayer::retailEffect(attack.animation);
    return attack.targetPattern == 9 || attack.targetSide >= 2 ||
           (!attack.targetSide &&
            (attack.targetPattern ? (main == 2 || main == 5 || main == 6 || main == 11)
                                  : (u8(attack.data, 0) >= 2 &&
                                     ((spell >= 32 && spell <= 35) || spell == 103))));
}
SpriteAsset effectSequence(const QVector<QImage> &images, const QVector<int> &holds,
                           const QString &name) {
    if (images.isEmpty() || images.size() != holds.size())
        throw Error("Wait for the complete effect preview");
    auto asset = SpriteAsset::blank(name.left(120), images.first().size());
    asset.kind = "sequence";
    asset.anchor = {asset.size.width() / 2, asset.size.height() / 2};
    asset.frames.clear();
    for (int i = 0; i < images.size(); ++i) {
        if (images[i].size() != asset.size || holds[i] < 1)
            throw Error("Invalid effect preview timing");
        for (int remaining = holds[i]; remaining > 0;) {
            const int hold = std::min(remaining, 255);
            remaining -= hold;
            const auto id = QString::number(asset.frames.size());
            asset.frames.append({"frame-" + id,
                                 "Frame " + QString::number(asset.frames.size() + 1),
                                 hold,
                                 {{"layer-" + id, "Rendered effect", true, {}, images[i]}}});
        }
    }
    asset.provenance = {
        {"source", "rendered-effect"}, {"framesPerSecond", 30}, {"effectRgba32", true}};
    asset.validate();
    return asset;
}
bool exportEffectSequence(const SpriteAsset &asset, const QString &path, EffectExport format,
                          QWidget *parent) {
    std::atomic_bool canceled = false;
    std::atomic_int completed = 0, total = 0;
    QProgressDialog dialog("Writing full effect sequence…", "Cancel", 0, 0, parent);
    dialog.setWindowTitle("Export effect");
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.setAutoClose(false);
    dialog.setAutoReset(false);
    QObject::connect(&dialog, &QProgressDialog::canceled, &dialog, [&] { canceled = true; });
    struct Result {
        bool saved;
        QString error;
    };
    QFutureWatcher<Result> watcher;
    QObject::connect(&watcher, &QFutureWatcher<Result>::finished, &dialog, &QDialog::accept);
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &dialog, [&] {
        if (total > 0) {
            dialog.setMaximum(total);
            dialog.setValue(completed);
        }
    });
    timer.start(50);
    watcher.setFuture(QtConcurrent::run([asset, path, format, &canceled, &completed, &total] {
        try {
            const auto progress = [&](int value, int maximum) {
                completed = value;
                total = maximum;
                return !canceled.load();
            };
            if (format == EffectExport::FramesZip)
                return Result{exportSpriteFramesZip(asset, path, 30, progress), {}};
            SpriteVideoOptions options{1, 1, Qt::transparent,
                                       format == EffectExport::Webm
                                           ? SpriteVideoFormat::Webm
                                           : SpriteVideoFormat::AnimatedPng,
                                       30};
            return Result{exportSpriteVideo(asset, path, options, progress), {}};
        } catch (const std::exception &e) {
            return Result{false, QString::fromUtf8(e.what())};
        }
    }));
    dialog.exec();
    if (!watcher.isFinished()) {
        canceled = true;
        watcher.waitForFinished();
    }
    const auto result = watcher.result();
    if (!result.error.isEmpty())
        throw Error(result.error);
    return result.saved;
}
QMap<int, QString> effectNames(const Rom &rom) {
    QMap<int, QString> names;
    const auto data = AttackData::loadRetail(rom);
    for (int i = 0; i <= 132; ++i) names[i] = QString("Effect ID %1").arg(i);
    for (int i = 1; i < 159; ++i) {
        const auto &a = data.entry(i);
        if (u8(a.data, 0) >= 2 && !AttackData::isTemplate(i))
            names[EffectPlayer::retailEffect(i)] = AttackData::displayName(a);
    }
    return names;
}
QImage effectChecker(const QImage &image) {
    QImage composed(image.size(), QImage::Format_RGBA8888);
    QPainter p(&composed);
    for (int y = 0; y < image.height(); y += 8)
        for (int x = 0; x < image.width(); x += 8)
            p.fillRect(x, y, 8, 8, ((x / 8 + y / 8) & 1) ? QColor("#303641") : QColor("#242932"));
    p.drawImage(0, 0, image);
    return composed;
}
QImage effectArtworkImage(const EffectArtwork &art, const AttackVisual &visual) {
    auto pixels = art.rgba;
    const auto apply = [&](int hue, int color) {
        const int mode = color >= 0 ? 2 : hue ? 1 : 0;
        if (mode) for (int at = 0; at < pixels.size(); at += 4)
            put32(pixels, at, lc_effect_color(u32(pixels, at), mode, hue, quint32(color)));
    };
    apply(visual.hue, visual.color);
    apply(art.hue, art.color);
    return QImage(reinterpret_cast<const uchar *>(pixels.constData()), art.width, art.height,
                  art.width * 4, QImage::Format_RGBA8888).copy();
}
EffectClip renderEffectClip(const Rom &rom, const AttackVisual &visual, int inherited,
                            const EffectPlayer::Scene &scene,
                            const std::shared_ptr<std::atomic_bool> &cancel) {
    EffectClip clip;
    try {
        EffectPlayer player(rom);
        player.startVisual(visual, inherited, scene);
        quint64 pixels = 0;
        while (player.advance()) {
            if (cancel->load()) return {};
            auto image = player.draw({320, 240}, .75).image;
            if (!clip.images.isEmpty() && clip.images.last() == image) ++clip.holds.last();
            else {
                pixels += 320 * 240;
                if (pixels > 64 * 1024 * 1024) throw Error("Effect preview exceeds its image budget");
                clip.images << image;
                clip.holds << 1;
            }
            ++clip.ticks;
        }
        for (const auto &r : player.resources()) {
            if (r.size.isEmpty() || r.size.width() > 256 || r.size.height() > 4096 ||
                (r.type != 4 && r.type != 7 && r.type != 8)) continue;
            const auto key = quint32(r.type << 16 | (r.id & 65535));
            const auto original = retailEffectResource(rom, r.type, r.id & 65535);
            if (original.size() == qint64(r.size.width()) * r.size.height() * 4)
                clip.artwork[key] = {r.size.width(), r.size.height(), original, sha256(original)};
        }
    } catch (const std::exception &e) { clip.error = QString::fromUtf8(e.what()); }
    return clip;
}
} // namespace lordly
