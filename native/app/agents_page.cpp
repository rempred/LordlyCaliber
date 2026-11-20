#include "app/agents_page.h"
#include "app/main_window.h"
#include "engine/agent_activity.h"
#include <QAction>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QUndoStack>
#include <QVBoxLayout>
namespace lordly {
namespace {
QPushButton *addButton(QHBoxLayout *row, const QString &title, const char *name) {
    auto button = new QPushButton(title);
    button->setObjectName(name);
    row->addWidget(button);
    return button;
}
} // namespace
AgentsPage::AgentsPage(Project *p, QUndoStack *undo, QWidget *parent)
    : QWidget(parent), project_(p), undo_(undo) {
    setObjectName("agentsPage");
    auto outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto body = new QWidget;
    auto layout = new QVBoxLayout(body);
    scroll->setWidget(body);
    outer->addWidget(scroll);
    auto connection = new QHBoxLayout;
    layout->addLayout(connection);
    auto label = new QLabel("CLI path");
    connection->addWidget(label);
    const QDir installation(QCoreApplication::applicationDirPath());
    const auto path = QDir::toNativeSeparators(installation.filePath("LordlyCaliber-cli.exe"));
    auto cli = new QLineEdit;
    cli->setObjectName("agentsCliPath");
    cli->setReadOnly(true);
    cli->setAccessibleName(label->text());
    cli->setToolTip(path);
    if (QFileInfo(path).isFile()) {
        cli->setText(path);
        cli->setCursorPosition(0);
        auto hint = new QLabel("The CLI comes with the editor. Paste the path above into your "
                               "agent’s chat and ask it to run the CLI on this computer with --guide.");
        hint->setWordWrap(true);
        layout->addWidget(hint);
    } else {
        cli->setPlaceholderText("CLI missing. Extract the full Windows ZIP to this folder.");
    }
    label->setBuddy(cli);
    connection->addWidget(cli, 1);
    auto guide = addButton(connection, "CLI guide and examples", "agentsReadGuide");
    const auto guidePath = installation.filePath("reference/agent-cli.md");
    connect(guide, &QPushButton::clicked, this, [this, guidePath] {
        QDialog dialog(this);
        dialog.setWindowTitle("CLI guide and examples");
        dialog.resize(840, 620);
        auto contents = new QVBoxLayout(&dialog);
        auto text = new QTextBrowser;
        text->setObjectName("agentsGuideText");
        text->setOpenExternalLinks(true);
        contents->addWidget(text);
        if (QFileInfo::exists(guidePath))
            text->setSource(QUrl::fromLocalFile(guidePath), QTextDocument::MarkdownResource);
        else
            text->setPlainText("The CLI guide is missing. Extract the reference folder beside "
                               "the editor executable.");
        auto close = new QDialogButtonBox(QDialogButtonBox::Close);
        contents->addWidget(close);
        connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        dialog.exec();
    });
    auto activity = new QGroupBox("CLI edits in this project");
    layout->addWidget(activity, 1);
    auto history = new QVBoxLayout(activity);
    auto help = new QLabel("Status shows whether an edit still matches the project. For artwork "
                           "and other compound edits, it compares the whole section.");
    help->setWordWrap(true);
    history->addWidget(help);
    edits_ = new QTreeWidget;
    edits_->setObjectName("agentsEdits");
    edits_->setRootIsDecorated(false);
    edits_->setHeaderLabels({"Edit", "Change", "Status", "Source", "Saved at"});
    edits_->setMinimumHeight(155);
    edits_->header()->setStretchLastSection(false);
    edits_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    edits_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int column = 2; column < edits_->columnCount(); ++column)
        edits_->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    history->addWidget(edits_);
    detail_ = new QPlainTextEdit;
    detail_->setObjectName("agentsEditDetails");
    detail_->setReadOnly(true);
    detail_->setMaximumHeight(100);
    history->addWidget(detail_);
    connect(edits_, &QTreeWidget::currentItemChanged, this, [this] { showDetails(); });
    auto actions = new QHBoxLayout;
    history->addLayout(actions);
    auto review = addButton(actions, "Project changelog", "agentsOpenChangelog");
    connect(review, &QPushButton::clicked, this, [this] {
        if (auto action = window()->findChild<QAction *>("reviewProjectChanges"))
            action->trigger();
    });
    auto patches = addButton(actions, "Project code patches", "agentsOpenPatches");
    connect(patches, &QPushButton::clicked, this, [this] {
        if (auto action = window()->findChild<QAction *>("showProjectCodePatches"))
            action->trigger();
    });
    actions->addStretch();
    connect(undo, &QUndoStack::indexChanged, this, [this] {
        if (isVisible())
            refresh();
    });
    connect(undo, &QUndoStack::cleanChanged, this, [this] {
        if (isVisible())
            refresh();
    });
}
void AgentsPage::refresh() {
    if (!project_)
        return;
    edits_->clear();
    auto main = qobject_cast<MainWindow *>(window());
    std::optional<Project> pending;
    if (main && main->agentSession() && !undo_->isClean())
        pending.emplace(main->activitySnapshot());
    const auto &current = pending ? *pending : *project_;
    displayedActivity_ = current.agentActivity();
    const QString pendingId = displayedActivity_.size() > project_->agentActivity().size()
                                  ? displayedActivity_.last().toObject()["id"].toString()
                                  : QString();
    QSet<QString> attributedPatches;
    for (const auto &v : agentEditRows(current)) {
        auto row = v.toObject();
        const auto scope = row["scope"].toObject();
        if (scope["kind"] == "patch")
            attributedPatches.insert(scope["key"].toString());
        const auto date =
            QDateTime::fromString(row["time"].toString(), Qt::ISODateWithMs).toLocalTime();
        auto item = new QTreeWidgetItem(
            edits_,
            {row["subject"].toString(), row["description"].toString(), row["status"].toString(),
             row["source"].toString(),
             row["batch"] == pendingId ? "Pending save" : date.toString("yyyy-MM-dd HH:mm:ss")});
        item->setData(0, Qt::UserRole, row["batch"].toString());
    }
    for (const auto &v : project_->codePatches()) {
        const auto p = v.toObject();
        const auto id = p["id"].toString();
        if (!attributedPatches.contains(id))
            new QTreeWidgetItem(edits_, {id,
                                         p["language"] == "c" ? "C patch" : "Assembly patch",
                                         p["enabled"].toBool(true) ? "Enabled" : "Disabled",
                                         "Origin unrecorded",
                                         {}});
    }
    if (edits_->topLevelItemCount())
        edits_->setCurrentItem(edits_->topLevelItem(0));
    else
        showDetails();
}
void AgentsPage::showDetails() {
    auto item = edits_->currentItem();
    if (!item) {
        detail_->setPlainText("No CLI activity was recorded in this project. Older projects can "
                              "still contain editable code patches.");
        return;
    }
    QStringList lines{item->text(0) + " · " + item->text(1), item->text(2)};
    for (const auto &v : displayedActivity_) {
        auto batch = v.toObject();
        if (batch["id"].toString() != item->data(0, Qt::UserRole).toString())
            continue;
        for (const auto &op : batch["actions"].toArray())
            lines << op.toString();
        for (const auto &d : batch["details"].toArray()) {
            const auto entry = d.toObject();
            lines << entry["area"].toString() + " · " + entry["subject"].toString();
            for (const auto &s : entry["details"].toArray())
                lines << "  " + s.toString();
        }
        break;
    }
    detail_->setPlainText(lines.join('\n'));
}
void AgentsPage::showEvent(QShowEvent *e) {
    QWidget::showEvent(e);
    refresh();
}
} // namespace lordly
