#include "app/agents_page.h"
#include "app/main_window.h"
#include "engine/agent_activity.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
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
QString executable() {
    return QDir::toNativeSeparators(
        QDir(QCoreApplication::applicationDirPath()).filePath("LordlyCaliber-cli.exe"));
}
QString quote(QString value) {
    return "'" + value.replace("'", "''") + "'";
}
QString guide() {
    return QDir(QCoreApplication::applicationDirPath()).filePath("reference/agent-cli.md");
}
QLineEdit *pathField(const char *name) {
    auto field = new QLineEdit;
    field->setObjectName(name);
    field->setReadOnly(true);
    return field;
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
    auto intro =
        new QLabel("Lordly Caliber’s command-line interface (CLI) lets an agent edit your project, "
                   "inspect game assets and matching code, and build ROMs with custom C or "
                   "assembly patches. It runs locally using files you choose.");
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto upper = new QHBoxLayout;
    layout->addLayout(upper);
    auto group = new QGroupBox("Connect an agent");
    auto left = new QVBoxLayout(group);
    auto form = new QFormLayout;
    auto cli = pathField("agentsCliPath");
    cli->setText(executable());
    rom_ = pathField("agentsRomPath");
    projectPath_ = pathField("agentsProjectPath");
    form->addRow("CLI executable", cli);
    form->addRow("Source ROM", rom_);
    form->addRow("Saved project", projectPath_);
    left->addLayout(form);
    status_ = new QLabel;
    status_->setObjectName("agentsConnectionStatus");
    status_->setWordWrap(true);
    left->addWidget(status_);
    auto buttons = new QHBoxLayout;
    left->addLayout(buttons);
    auto addButton = [](QHBoxLayout *row, const QString &title, const char *name) {
        auto button = new QPushButton(title);
        button->setObjectName(name);
        row->addWidget(button);
        return button;
    };
    connect(addButton(buttons, "Copy connection", "agentsCopyConnection"), &QPushButton::clicked,
            this, [this] { QApplication::clipboard()->setText(connection()); });
    auto projectButtons = new QHBoxLayout;
    left->addLayout(projectButtons);
    connect(addButton(projectButtons, "Save project…", "agentsSaveProject"), &QPushButton::clicked,
            this, [this] {
                if (auto action = window()->findChild<QAction *>("saveSharingArchive"))
                    action->trigger();
            });
    connect(addButton(projectButtons, "Open agent project…", "agentsOpenProject"),
            &QPushButton::clicked, this, [this] {
                if (auto action = window()->findChild<QAction *>("openSavedProject"))
                    action->trigger();
            });
    connect(addButton(buttons, "Read CLI guide", "agentsReadGuide"), &QPushButton::clicked, this,
            [this] {
                QDialog dialog(this);
                dialog.setWindowTitle("Portable CLI guide");
                dialog.resize(840, 620);
                auto layout = new QVBoxLayout(&dialog);
                auto text = new QTextBrowser;
                layout->addWidget(text);
                if (QFileInfo::exists(guide()))
                    text->setSource(QUrl::fromLocalFile(guide()), QTextDocument::MarkdownResource);
                else
                    text->setPlainText("The guide is missing. Place the release’s reference folder "
                                       "beside the editor executable.");
                text->setSearchPaths({QFileInfo(guide()).absolutePath()});
                auto close = new QDialogButtonBox(QDialogButtonBox::Close);
                layout->addWidget(close);
                connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
                dialog.exec();
            });
    left->addStretch();
    upper->addWidget(group, 1);
    auto prompt = new QGroupBox("Prompt your agent");
    auto right = new QVBoxLayout(prompt);
    auto examples = new QComboBox;
    examples->setObjectName("agentsPromptExample");
    examples->addItem("Edit game data",
                      "Set the base strength of the Knight class to 300. Show me the original "
                      "value, save the edited project and verify the exported ROM.");
    examples->addItem("Create a code patch",
                      "Make Heal Leaf restore half of the target’s missing HP, capped at 60. Trace "
                      "the game’s prediction and application code. Keep the C or assembly patch "
                      "editable in the project and verify the result.");
    examples->addItem("Inspect a project",
                      "List the agent edits and code patches saved in this project. Explain which "
                      "values still match and identify the game features they affect.");
    right->addWidget(examples);
    task_ = new QPlainTextEdit;
    task_->setObjectName("agentsPromptTask");
    task_->setPlainText(examples->currentData().toString());
    task_->setAccessibleName("Task for your agent");
    task_->setMaximumHeight(105);
    right->addWidget(task_);
    connect(examples, &QComboBox::currentIndexChanged, this,
            [this, examples] { task_->setPlainText(examples->currentData().toString()); });
    auto copy = new QPushButton("Copy prompt with connection");
    copy->setObjectName("agentsCopyPrompt");
    right->addWidget(copy, 0, Qt::AlignLeft);
    connect(copy, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(connection() + "\n\n" + task_->toPlainText().trimmed());
    });
    upper->addWidget(prompt, 1);
    auto activity = new QGroupBox("CLI edits in this project");
    layout->addWidget(activity, 1);
    auto history = new QVBoxLayout(activity);
    auto help = new QLabel("Saved CLI batches and workspace edits appear here and in the project "
                           "changelog. Status compares individual record fields and code patches, "
                           "or the whole saved section for compound edits.");
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
QString AgentsPage::connection() const {
    QStringList lines{"Use the local Lordly Caliber CLI at " + executable(),
                      "Read its shipped guide in PowerShell with & " + quote(executable()) +
                          " --guide",
                      "Discover commands with & " + quote(executable()) + " --capabilities",
                      "Source ROM: " + QDir::toNativeSeparators(project_->rom().sourcePath())};
    if (!project_->projectPath().isEmpty())
        lines << "Input project: " + QDir::toNativeSeparators(project_->projectPath());
    if (!undo_->isClean())
        lines << "The editor has unsaved changes. Use the newly saved project for these edits.";
    if (project_->projectPath().isEmpty())
        lines << "There is no saved project path yet. Use the ROM for a new project, or supply a "
                 "saved project to continue editor work.";
    lines << "Use the source ROM as input. Save edits and editable patch source to a new project "
             "and export the test ROM to a separate file.";
    return lines.join('\n');
}
void AgentsPage::refresh() {
    if (!project_)
        return;
    rom_->setText(QDir::toNativeSeparators(project_->rom().sourcePath()));
    projectPath_->setText(QDir::toNativeSeparators(project_->projectPath()));
    for (auto field : {rom_, projectPath_}) {
        field->setCursorPosition(0);
        field->setToolTip(field->text());
    }
    projectPath_->setPlaceholderText("No saved project selected");
    status_->setText(!QFileInfo::exists(executable())
                         ? "The portable CLI is missing beside the editor executable."
                     : !undo_->isClean()
                         ? "Save your project to give the agent your current edits. Open the "
                           "agent’s saved result to review its changes."
                         : "The agent starts a separate local process. Open its saved project here "
                           "to review the result.");
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
