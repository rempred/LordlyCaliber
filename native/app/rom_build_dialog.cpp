#include "app/rom_build_dialog.h"
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

namespace lordly {
RomBuildDialog::RomBuildDialog(const QString &path, QWidget *parent)
    : QDialog(parent), path_(path) {
    setObjectName("romBuildDialog");
    setWindowTitle("Building ROM");
    setWindowModality(Qt::WindowModal);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    resize(700, 500);
    auto layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    heading_ = new QLabel("Building ROM");
    heading_->setObjectName("romBuildHeading");
    layout->addWidget(heading_);
    auto destination = new QLabel(QFileInfo(path).fileName());
    destination->setTextFormat(Qt::PlainText);
    destination->setTextInteractionFlags(Qt::TextSelectableByMouse);
    destination->setToolTip(path);
    destination->setWordWrap(true);
    layout->addWidget(destination);
    layout->addWidget(new QLabel("Overall progress"));
    overall_ = new QProgressBar;
    overall_->setObjectName("romBuildOverall");
    overall_->setAccessibleName("Overall build progress");
    overall_->setRange(0, BuildProgress::stageCount);
    overall_->setValue(0);
    overall_->setFormat("%v of %m stages complete");
    overall_->setMinimumHeight(24);
    layout->addWidget(overall_);
    stage_ = new QLabel("Preparing project");
    stage_->setObjectName("romBuildStage");
    layout->addWidget(stage_);
    current_ = new QProgressBar;
    current_->setObjectName("romBuildCurrent");
    current_->setAccessibleName("Current build activity");
    current_->setRange(0, 0);
    current_->setMinimumHeight(18);
    current_->setTextVisible(false);
    layout->addWidget(current_);
    activity_ = new QLabel("Starting…");
    activity_->setTextFormat(Qt::PlainText);
    activity_->setWordWrap(true);
    layout->addWidget(activity_);
    auto reportRow = new QHBoxLayout;
    reportRow->addWidget(new QLabel("Build report"));
    reportRow->addStretch();
    elapsed_ = new QLabel;
    reportRow->addWidget(elapsed_);
    layout->addLayout(reportRow);
    log_ = new QPlainTextEdit;
    log_->setObjectName("romBuildLog");
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(2000);
    log_->setMinimumHeight(140);
    layout->addWidget(log_, 1);
    auto buttons = new QHBoxLayout;
    auto copy = new QPushButton("Copy report");
    copy->setObjectName("romBuildCopy");
    buttons->addWidget(copy);
    connect(copy, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(path_ + "\n\n" + log_->toPlainText());
    });
    buttons->addStretch();
    review_ = new QPushButton("Review project changes");
    review_->setObjectName("romBuildReview");
    review_->hide();
    buttons->addWidget(review_);
    connect(review_, &QPushButton::clicked, this, [this] {
        accept();
        emit reviewRequested();
    });
    test_ = new QPushButton("Test this ROM");
    test_->setObjectName("romBuildTest");
    test_->hide();
    buttons->addWidget(test_);
    connect(test_, &QPushButton::clicked, this, [this] {
        accept();
        emit testRequested();
    });
    close_ = new QPushButton("Close");
    close_->setObjectName("romBuildClose");
    close_->setEnabled(false);
    buttons->addWidget(close_);
    connect(close_, &QPushButton::clicked, this, &QDialog::accept);
    layout->addLayout(buttons);
    clock_.start();
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &RomBuildDialog::updateElapsed);
    timer_->start(1000);
    updateElapsed();
}
BuildProgressCallback RomBuildDialog::progressCallback() {
    // The window remains alive until the worker finishes. Queued updates never
    // touch widgets on the worker thread and are dropped if the owner goes away.
    return [guard = QPointer<RomBuildDialog>(this)](const BuildProgress &progress) {
        if (guard)
            QMetaObject::invokeMethod(guard.data(), [guard, progress] {
                if (guard) guard->updateProgress(progress);
            }, Qt::QueuedConnection);
    };
}
void RomBuildDialog::updateProgress(const BuildProgress &progress) {
    if (!running_) return;
    overall_->setValue(qBound(overall_->value(), progress.stage, BuildProgress::stageCount));
    stage_->setText(progress.title);
    activity_->setText(progress.activity.isEmpty() ? "Working…" : progress.activity);
    current_->setRange(0, qMax(0, progress.total));
    current_->setTextVisible(progress.total > 0);
    current_->setFormat("%v / %m");
    if (progress.total > 0) current_->setValue(qBound(0, progress.completed, progress.total));
    for (const auto &message : progress.messages) appendMessage(message);
}
void RomBuildDialog::appendMessage(const QString &message) {
    auto scroll = log_->verticalScrollBar();
    const bool follow = scroll->value() >= scroll->maximum();
    const auto seconds = clock_.elapsed() / 1000;
    log_->appendPlainText(QString("[%1:%2] %3").arg(seconds / 60)
        .arg(seconds % 60, 2, 10, QChar('0')).arg(message));
    if (follow) scroll->setValue(scroll->maximum());
}
void RomBuildDialog::updateElapsed() {
    const auto seconds = clock_.elapsed() / 1000;
    elapsed_->setText(QString("Elapsed %1:%2").arg(seconds / 60)
        .arg(seconds % 60, 2, 10, QChar('0')));
}
void RomBuildDialog::finish(const QString &error, bool canTest, bool canReview) {
    if (!running_) return;
    running_ = false;
    timer_->stop();
    updateElapsed();
    const bool success = error.isEmpty();
    heading_->setText(success ? "ROM exported" : "ROM build failed");
    setWindowTitle(heading_->text());
    current_->setRange(0, 1);
    current_->setTextVisible(false);
    current_->setValue(success ? 1 : 0);
    if (success) {
        overall_->setValue(BuildProgress::stageCount);
        stage_->setText("Complete");
        activity_->setText("Saved to " + path_);
        appendMessage("Finished: " + path_);
    } else {
        activity_->setText("The ROM was not exported. See the error below.");
        appendMessage("ERROR: " + error);
        log_->verticalScrollBar()->setValue(log_->verticalScrollBar()->maximum());
    }
    test_->setVisible(success && canTest);
    review_->setVisible(!success && canReview);
    close_->setEnabled(true);
    close_->setDefault(true);
    close_->setFocus();
}
void RomBuildDialog::done(int result) {
    if (!running_) QDialog::done(result);
}
void RomBuildDialog::closeEvent(QCloseEvent *event) {
    if (running_) event->ignore();
    else QDialog::closeEvent(event);
}
}
