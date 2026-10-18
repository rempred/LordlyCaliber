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
void AnimationSequenceEditor::chooseWebmExport() {
    const auto name = QString("class-%1-animation-%2.webm")
                          .arg(selectedClass())
                          .arg(poses_->currentData().toInt());
    const auto path = QFileDialog::getSaveFileName(this, "Export displayed frame sequence", name,
                                                   "WebM video (*.webm)");
    if (path.isEmpty())
        return;
    const bool saved = exportWebm(path);
    notice_->setText(saved ? "Sequence exported to " + path : "Sequence export canceled.");
    notice_->show();
}
bool AnimationSequenceEditor::exportWebm(const QString &path) {
    checkDestination(*project_, path);
    if (!isEnabled() || !frames_ || !frames_->count())
        throw Error("Choose a combat animation first");
    if (auto play = preview_->findChild<QPushButton *>("actorPlay"))
        play->setChecked(false);
    const auto asset = preview_->poseAsset();
    const SpriteVideoOptions options{1, 1, Qt::black, SpriteVideoFormat::Webm};
    const auto plan = spriteVideoPlan(asset, options);
    std::atomic_bool canceled = false;
    std::atomic_int completed = 0;
    QProgressDialog dialog("Exporting frame sequence…", "Cancel", 0, plan.ticks, this);
    dialog.setWindowTitle("Export Sequence WebM");
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
    connect(&timer, &QTimer::timeout, &dialog, [&] { dialog.setValue(completed.load()); });
    timer.start(50);
    watcher.setFuture(QtConcurrent::run([asset, path, options, &canceled, &completed] {
        try {
            return Result{exportSpriteVideo(asset, path, options,
                                            [&](int value, int) {
                                                completed = value;
                                                return !canceled.load();
                                            }),
                          {}};
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
