#include "app/project_review_dialog.h"
#include <QDialogButtonBox>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace lordly {
ProjectReviewDialog::ProjectReviewDialog(Project snapshot, const EditLocation &query, QWidget *parent)
    : QDialog(parent) {
    const bool exporting = query.editor.isEmpty();
    setObjectName(exporting ? "exportSummary" : "projectUses");
    setWindowTitle(exporting ? "Review ROM export" : "Where is this used?");
    resize(1040, 710);
    auto layout = new QVBoxLayout(this);
    auto status = new QLabel(exporting ? "Checking project changes and references…" : "Finding uses…");
    status->setObjectName("projectReviewStatus"); status->setWordWrap(true);
    layout->addWidget(status);
    auto search = new QLineEdit;
    search->setPlaceholderText("Filter results…"); search->setClearButtonEnabled(true);
    search->setObjectName("projectReviewSearch");
    layout->addWidget(search);
    auto split = new QSplitter;
    auto rows = new QTreeWidget;
    rows->setObjectName("projectReviewEntries"); rows->setRootIsDecorated(false);
    rows->setHeaderLabels({"Section", "Entry"});
    rows->setColumnWidth(0, 135); rows->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    auto details = new QPlainTextEdit;
    details->setObjectName("projectReviewDetails"); details->setReadOnly(true);
    split->addWidget(rows); split->addWidget(details); split->setSizes({520, 500});
    layout->addWidget(split, 1);
    auto scope = new QLabel(exporting
        ? "Review the changes below. Compilation also checks code sizes, allocations and ROM checksums before writing the output."
        : "Known uses in editable records, inventory, squads, treasures and animation assignments. Story events that calculate a value during play cannot be resolved here. Art Library assignments are independent copies, not live links.");
    scope->setWordWrap(true); layout->addWidget(scope);
    auto buttons = new QDialogButtonBox(exporting ? QDialogButtonBox::Cancel : QDialogButtonBox::Close);
    auto open = buttons->addButton("Open selected edit", QDialogButtonBox::ActionRole);
    open->setObjectName("openReviewEdit"); open->setEnabled(false);
    QPushButton *proceed = nullptr;
    if (exporting) {
        proceed = buttons->addButton("Export ROM…", QDialogButtonBox::AcceptRole);
        proceed->setObjectName("confirmRomExport"); proceed->setEnabled(false);
        connect(proceed, &QPushButton::clicked, this, &QDialog::accept);
    }
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(rows, &QTreeWidget::currentItemChanged, this, [details, open](QTreeWidgetItem *row) {
        details->setPlainText(row ? row->data(0, Qt::UserRole + 1).toString() : QString());
        open->setEnabled(row && !row->data(0, Qt::UserRole).value<EditLocation>().editor.isEmpty());
    });
    const auto jump = [this, rows] {
        auto row = rows->currentItem();
        if (!row) return;
        const auto target = row->data(0, Qt::UserRole).value<EditLocation>();
        if (target.editor.isEmpty()) return;
        reject();
        emit editRequested(target);
    };
    connect(open, &QPushButton::clicked, this, jump);
    connect(rows, &QTreeWidget::itemDoubleClicked, this, [jump] { jump(); });
    connect(search, &QLineEdit::textChanged, this, [rows](const QString &text) {
        for (int i = 0; i < rows->topLevelItemCount(); ++i) {
            auto row = rows->topLevelItem(i);
            row->setHidden(!(row->text(0) + row->text(1) + row->data(0, Qt::UserRole + 1).toString()).contains(text, Qt::CaseInsensitive));
        }
        if (rows->currentItem() && rows->currentItem()->isHidden()) rows->setCurrentItem(nullptr);
    });
    struct Result { QVector<ProjectChange> changes; QVector<ExportIssue> issues; QString source, error; };
    auto watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [=] {
        const auto result = watcher->result(); watcher->deleteLater();
        if (!result.error.isEmpty()) { status->setText("Review could not finish: " + result.error); return; }
        auto add = [rows](const QString &section, const QString &subject, const QString &text, const EditLocation &target) {
            auto row = new QTreeWidgetItem(rows, {section, subject});
            row->setData(0, Qt::UserRole, QVariant::fromValue(target));
            row->setData(0, Qt::UserRole + 1, text); row->setToolTip(1, text);
            return row;
        };
        int errors = 0;
        for (const auto &issue : result.issues) {
            errors += issue.blocking;
            auto row = add(issue.blocking ? "Needs fixing" : "Source warning", issue.message,
                          issue.message + (issue.blocking ? "\nOpen this edit to correct it, then review the export again." : "\nThis unresolved reference was already in the source ROM."), issue.location);
            row->setForeground(0, issue.blocking ? QColor("#a12620") : QColor("#86611e"));
        }
        for (const auto &change : result.changes)
            add(change.area, change.subject, change.subject + "\n\n" + change.details.join("\n"), change.location);
        if (exporting) {
            status->setText(QString("Changed entries: %1 · Problems preventing export: %2 · Compared with %3")
                .arg(result.changes.size()).arg(errors).arg(result.source));
            proceed->setEnabled(errors == 0);
        } else status->setText(QString("%1 known uses found.").arg(result.changes.size()));
        if (rows->topLevelItemCount()) rows->setCurrentItem(rows->topLevelItem(0));
        else details->setPlainText(exporting ? "No project changes or broken references found. Export still rebuilds supported game code from the bundled sources."
                                            : "No references found in the supported editable records. A copied library image has no live assignments.");
        setProperty("reviewReady", true);
    });
    watcher->setFuture(QtConcurrent::run([snapshot = std::move(snapshot), query, exporting]() mutable {
        try {
            if (!exporting) return Result{findProjectUses(std::move(snapshot), query.editor, query.id, query.key), {}, {}, {}};
            const auto report = snapshot.changeReport();
            return Result{report.entries, exportIssues(std::move(snapshot)), report.source, {}};
        } catch (const std::exception &e) { return Result{{}, {}, {}, QString::fromUtf8(e.what())}; }
    }));
}
} // namespace lordly
