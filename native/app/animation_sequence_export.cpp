#include "app/actor_pose_editor.h"
#include "app/animation_sequence_editor.h"
#include "engine/sprite_video.h"
#include <QBuffer>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLabel>
#include <QListWidget>
#include <QProgressDialog>
#include <QPushButton>
#include <QTimer>
#include <QtConcurrent>
#include <atomic>

namespace lordly {
namespace {
void checkDestination(const Project &project, const QString &path) {
    if (path.isEmpty())
        throw Error("Choose an export destination");
    if (QFileInfo(path).exists() && QFileInfo(path).canonicalFilePath() ==
                                        QFileInfo(project.rom().sourcePath()).canonicalFilePath())
        throw Error("Cannot overwrite the source ROM");
}
} // namespace
void AnimationSequenceEditor::chooseFrameExport() {
    const auto name = QString("class-%1-animation-%2-frame-%3.png")
                          .arg(selectedClass())
                          .arg(poses_->currentData().toInt())
                          .arg(frames_->currentRow() + 1);
    const auto path =
        QFileDialog::getSaveFileName(this, "Export selected frame", name, "PNG image (*.png)");
    if (path.isEmpty())
        return;
    exportFrame(path);
    notice_->setText("Frame exported to " + path);
    notice_->show();
}
void AnimationSequenceEditor::exportFrame(const QString &path) const {
    checkDestination(*project_, path);
    if (!isEnabled() || !frames_ || frames_->currentRow() < 0)
        throw Error("Choose a combat animation frame first");
    const auto asset = preview_->poseAsset();
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!asset.composite(frames_->currentRow()).save(&buffer, "PNG"))
        throw Error("Cannot encode animation frame");
    atomicWrite(path, bytes);
}
void AnimationSequenceEditor::chooseAnimationExport(AnimationExport format) {
    const QString extension = format == AnimationExport::AnimatedPng ? "apng" :
                              format == AnimationExport::FramesZip ? "zip" : "webm";
    const QString filter = format == AnimationExport::AnimatedPng ? "Animated PNG (*.apng)" :
                           format == AnimationExport::FramesZip ? "PNG frame sequence (*.zip)" : "WebM video (*.webm)";
    const auto name = QString("class-%1-animation-%2.%3")
                          .arg(selectedClass())
                          .arg(poses_->currentData().toInt()).arg(extension);
    const auto path = QFileDialog::getSaveFileName(this, "Export preview animation", name, filter);
    if (path.isEmpty())
        return;
    const bool saved = exportAnimation(path, format);
    notice_->setText(saved ? "Sequence exported to " + path : "Sequence export canceled.");
    notice_->show();
}
bool AnimationSequenceEditor::exportWebm(const QString &path) {
    return exportAnimation(path, AnimationExport::Webm);
}
bool AnimationSequenceEditor::exportAnimation(const QString &path, AnimationExport format) {
    checkDestination(*project_, path);
    if (!isEnabled() || !frames_ || !frames_->count())
        throw Error("Choose a combat animation first");
    const auto source = preview_->playbackSource();
    std::atomic_bool canceled = false;
    std::atomic_int completed = 0, total = 0;
    QProgressDialog dialog("Rendering preview playback…", "Cancel", 0, 0, this);
    dialog.setWindowTitle("Export animation");
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.setAutoClose(false);
    dialog.setAutoReset(false);
    connect(&dialog, &QProgressDialog::canceled, &dialog, [&] { canceled = true; });
    struct Result {
        bool saved;
        QString error;
    };
    QFutureWatcher<Result> watcher;
    connect(&watcher, &QFutureWatcher<Result>::finished, &dialog, &QDialog::accept);
    QTimer timer;
    connect(&timer, &QTimer::timeout, &dialog, [&] {
        if (total.load() > 0) {
            dialog.setLabelText("Writing animation…");
            dialog.setMaximum(total.load());
            dialog.setValue(completed.load());
        }
    });
    timer.start(50);
    watcher.setFuture(QtConcurrent::run([source, path, format, &canceled, &completed, &total] {
        try {
            const auto asset = source.capture([&](int) { return !canceled.load(); });
            if (!asset) return Result{false, {}};
            const auto progress = [&](int value, int maximum) {
                total = maximum; completed = value;
                return !canceled.load();
            };
            if (format == AnimationExport::FramesZip)
                return Result{exportSpriteFramesZip(*asset, path, source.stepsPerSecond, progress), {}};
            const SpriteVideoOptions options{1, 1, Qt::transparent,
                format == AnimationExport::AnimatedPng ? SpriteVideoFormat::AnimatedPng : SpriteVideoFormat::Webm,
                source.stepsPerSecond};
            return Result{exportSpriteVideo(*asset, path, options, progress), {}};
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
} // namespace lordly
