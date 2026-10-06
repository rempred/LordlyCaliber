#include "app/change_report_dialog.h"
#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace lordly {
ChangeReportDialog::ChangeReportDialog(Project snapshot, QWidget *parent) : QDialog(parent) {
    sourcePath_ = snapshot.rom().sourcePath();
    setObjectName("projectChanges");
    setWindowTitle("Review project changes");
    resize(1020, 720);
    auto layout = new QVBoxLayout(this);
    auto status = new QLabel("Collecting project changes…");
    status->setObjectName("changeReportStatus");
    layout->addWidget(status);
    auto search = new QLineEdit;
    search->setObjectName("changeReportSearch");
    search->setPlaceholderText("Find in report… (Enter for next match)");
    layout->addWidget(search);
    text_ = new QPlainTextEdit;
    text_->setObjectName("changeReportText");
    text_->setReadOnly(true);
    layout->addWidget(text_, 1);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    auto copy = buttons->addButton("Copy report", QDialogButtonBox::ActionRole);
    copy->setObjectName("copyChangeReport");
    auto save = buttons->addButton("Save text…", QDialogButtonBox::ActionRole);
    save->setObjectName("saveChangeReport");
    copy->setEnabled(false);
    save->setEnabled(false);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(copy, &QPushButton::clicked, this, [this] {
        if (ready_)
            QApplication::clipboard()->setText(text_->toPlainText());
    });
    connect(save, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getSaveFileName(
            this, "Save project changelog", "Lordly-Caliber-changes.txt", "Text (*.txt)");
        if (path.isEmpty())
            return;
        try {
            saveReport(path);
        } catch (const std::exception &e) {
            QMessageBox::warning(this, "Cannot save report", QString::fromUtf8(e.what()));
        }
    });
    auto find = [this, search] {
        if (!text_->find(search->text())) {
            text_->moveCursor(QTextCursor::Start);
            text_->find(search->text());
        }
    };
    connect(search, &QLineEdit::returnPressed, this, find);
    connect(search, &QLineEdit::textChanged, this, [this, find] {
        text_->moveCursor(QTextCursor::Start);
        find();
    });
    struct Result {
        ChangeReport report;
        QString error;
    };
    auto watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher, status, copy, save] {
        const auto result = watcher->result();
        watcher->deleteLater();
        if (!result.error.isEmpty()) {
            status->setText("Could not collect project changes.");
            text_->setPlainText(result.error);
            return;
        }
        text_->setPlainText(result.report.text());
        text_->moveCursor(QTextCursor::Start);
        status->setText(
            QString("Report entries: %1").arg(result.report.entries.size()));
        ready_ = true;
        copy->setEnabled(true);
        save->setEnabled(true);
    });
    watcher->setFuture(QtConcurrent::run([snapshot = std::move(snapshot)] {
        try {
            return Result{snapshot.changeReport(), {}};
        } catch (const std::exception &e) {
            return Result{{}, QString::fromUtf8(e.what())};
        }
    }));
}
void ChangeReportDialog::saveReport(const QString &path) const {
    if (!ready_)
        throw Error("Wait for the report to finish before saving");
    const QFileInfo target(path), source(sourcePath_);
    if (!sourcePath_.isEmpty() &&
        (target.absoluteFilePath().compare(source.absoluteFilePath(), Qt::CaseInsensitive) == 0 ||
         (!target.canonicalFilePath().isEmpty() &&
          target.canonicalFilePath().compare(source.canonicalFilePath(), Qt::CaseInsensitive) ==
              0)))
        throw Error("Choose a report path separate from the source ROM");
    atomicWrite(path, text_->toPlainText().toUtf8());
}
} // namespace lordly
