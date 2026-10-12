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
#include <QTreeWidget>
#include <QHeaderView>
#include <QSplitter>
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
    auto split = new QSplitter;
    entries_ = new QTreeWidget;
    entries_->setObjectName("changeReportEntries");
    entries_->setHeaderLabels({"Area", "Edit"});
    entries_->setRootIsDecorated(false);
    entries_->setUniformRowHeights(true);
    entries_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    entries_->setColumnWidth(0, 140);
    split->addWidget(entries_);
    text_ = new QPlainTextEdit;
    text_->setObjectName("changeReportText");
    text_->setReadOnly(true);
    split->addWidget(text_);
    split->setSizes({380, 620});
    layout->addWidget(split, 1);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    auto jump = buttons->addButton("Open selected edit", QDialogButtonBox::ActionRole);
    jump->setObjectName("openChangeEdit");
    jump->setEnabled(false);
    const auto open = [this] {
        const auto row = entries_->currentItem();
        if (!row) return;
        const auto location = row->data(0, Qt::UserRole).value<EditLocation>();
        accept();
        emit editRequested(location);
    };
    connect(jump, &QPushButton::clicked, this, open);
    connect(entries_, &QTreeWidget::itemDoubleClicked, this, [open] { open(); });
    connect(entries_, &QTreeWidget::currentItemChanged, this, [this, jump](QTreeWidgetItem *row) {
        jump->setEnabled(row != nullptr);
        if (!row) return;
        text_->moveCursor(QTextCursor::Start);
        text_->find(row->text(1));
    });
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
    connect(search, &QLineEdit::textChanged, this, [this, find, search] {
        for (int i = 0; i < entries_->topLevelItemCount(); ++i) {
            const auto row = entries_->topLevelItem(i);
            row->setHidden(!row->data(0, Qt::UserRole + 1).toString().contains(search->text(), Qt::CaseInsensitive));
        }
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
        report_ = result.report;
        for (const auto &entry : report_.entries) {
            auto row = new QTreeWidgetItem(entries_, {entry.area, entry.subject});
            row->setData(0, Qt::UserRole, QVariant::fromValue(entry.location));
            row->setData(0, Qt::UserRole + 1, entry.area + " " + entry.subject + " " + entry.details.join(" "));
            row->setToolTip(1, entry.details.join('\n'));
        }
        text_->moveCursor(QTextCursor::Start);
        status->setText(
            QString("%1 changed entries · compared with %2. Select an entry to review or open it.")
                .arg(result.report.entries.size()).arg(result.report.source));
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
