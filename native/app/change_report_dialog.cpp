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
ChangeReportDialog::ChangeReportDialog(Project snapshot, QWidget *parent) : QDialog(parent), snapshot_(std::move(snapshot)) {
    sourcePath_ = snapshot_.rom().sourcePath();
    setObjectName("projectChanges");
    setWindowTitle("Review project changes");
    resize(1020, 720);
    auto layout = new QVBoxLayout(this);
    auto status = status_ = new QLabel("Collecting project changes…");
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
        if (location.editor.isEmpty()) return;
        accept();
        emit editRequested(location);
    };
    connect(jump, &QPushButton::clicked, this, open);
    connect(entries_, &QTreeWidget::itemDoubleClicked, this, [open] { open(); });
    connect(entries_, &QTreeWidget::currentItemChanged, this, [this, jump](QTreeWidgetItem *row) {
        jump->setEnabled(row && !row->data(0, Qt::UserRole).value<EditLocation>().editor.isEmpty());
        if (!row) return;
        text_->moveCursor(QTextCursor::Start);
        text_->find(row->text(1));
    });
    auto copy = copy_ = buttons->addButton("Copy report", QDialogButtonBox::ActionRole);
    copy->setObjectName("copyChangeReport");
    auto save = save_ = buttons->addButton("Save text…", QDialogButtonBox::ActionRole);
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
        if (entries_->currentItem() && entries_->currentItem()->isHidden()) entries_->setCurrentItem(nullptr);
        text_->moveCursor(QTextCursor::Start);
        find();
    });
    compare_ = buttons->addButton("Compare with retail ROM…", QDialogButtonBox::ActionRole);
    compare_->setObjectName("compareRetailRom");
    compare_->setToolTip("Include edits already present in the opened ROM by comparing with your unmodified ROM. Neither file is changed.");
    source_ = buttons->addButton("Use opened ROM", QDialogButtonBox::ActionRole);
    source_->setObjectName("compareSourceRom");
    connect(compare_, &QPushButton::clicked, this, [this] {
        const auto path=QFileDialog::getOpenFileName(this,"Choose an unmodified comparison ROM",{},"Nintendo 64 ROM (*.v64 *.z64 *.n64)");
        if (!path.isEmpty()) compareWithRom(path);
    });
    connect(source_, &QPushButton::clicked, this, [this] { compareWithRom({}); });
    compareWithRom({});
}
void ChangeReportDialog::compareWithRom(const QString &path) {
    if (property("collecting").toBool()) return;
    setProperty("collecting",true); ready_=false;
    comparisonPath_=path;
    copy_->setEnabled(false); save_->setEnabled(false); compare_->setEnabled(false); source_->setEnabled(false);
    entries_->clear(); text_->clear();
    findChild<QLineEdit *>("changeReportSearch")->clear();
    status_->setText(path.isEmpty() ? "Collecting project changes…" : "Comparing editable data and artwork with the reference ROM…");
    struct Result {
        ChangeReport report;
        QString error;
    };
    auto watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher] {
        setProperty("collecting",false); compare_->setEnabled(true); source_->setEnabled(!comparisonPath_.isEmpty());
        const auto result = watcher->result();
        watcher->deleteLater();
        if (!result.error.isEmpty()) {
            status_->setText("Could not collect project changes.");
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
        status_->setText(
            QString("%1 changed entries · compared with %2. Select an entry to review or open it.")
                .arg(result.report.entries.size()).arg(result.report.source));
        ready_ = true;
        copy_->setEnabled(true);
        save_->setEnabled(true);
    });
    watcher->setFuture(QtConcurrent::run([snapshot = snapshot_, path] {
        try {
            return Result{path.isEmpty() ? snapshot.changeReport() : snapshot.compareWithRom(Rom::open(path)), {}};
        } catch (const std::exception &e) {
            return Result{{}, QString::fromUtf8(e.what())};
        }
    }));
}
void ChangeReportDialog::saveReport(const QString &path) const {
    if (!ready_)
        throw Error("Wait for the report to finish before saving");
    const QFileInfo target(path), source(sourcePath_);
    if (!comparisonPath_.isEmpty() && (target.absoluteFilePath().compare(QFileInfo(comparisonPath_).absoluteFilePath(), Qt::CaseInsensitive)==0 || (!target.canonicalFilePath().isEmpty() && target.canonicalFilePath().compare(QFileInfo(comparisonPath_).canonicalFilePath(),Qt::CaseInsensitive)==0)))
        throw Error("Choose a report path separate from the comparison ROM");
    if (!sourcePath_.isEmpty() &&
        (target.absoluteFilePath().compare(source.absoluteFilePath(), Qt::CaseInsensitive) == 0 ||
         (!target.canonicalFilePath().isEmpty() &&
          target.canonicalFilePath().compare(source.canonicalFilePath(), Qt::CaseInsensitive) ==
              0)))
        throw Error("Choose a report path separate from the source ROM");
    atomicWrite(path, text_->toPlainText().toUtf8());
}
} // namespace lordly
