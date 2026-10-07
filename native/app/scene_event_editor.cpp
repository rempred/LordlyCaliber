#include "app/view_position.h"
#include "app/scene_event_editor.h"
#include <QComboBox>
#include <QCompleter>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QUndoStack>
#include <QVBoxLayout>
namespace lordly {
namespace {
class EventCommand final : public QUndoCommand {
  public:
    EventCommand(Project *project, QJsonObject after)
        : QUndoCommand("Change scene launch"), project_(project),
          before_(project->sceneEvents()->json()), after_(std::move(after)) {}
    void undo() override {
        project_->setSceneEvents(before_);
    }
    void redo() override {
        project_->setSceneEvents(after_);
    }

  private:
    Project *project_;
    QJsonObject before_, after_;
};
} // namespace
SceneEventEditor::SceneEventEditor(Project *project, QUndoStack *undo,
                                   std::function<void()> changed, QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("sceneEventEditor");
    auto layout = new QVBoxLayout(this);
    auto note = new QLabel("Choose the scene launched by a game event. Shared launch sites affect "
                           "every event route that uses them.");
    note->setWordWrap(true);
    layout->addWidget(note);
    search_ = new QLineEdit;
    search_->setObjectName("sceneEventSearch");
    search_->setPlaceholderText("Search launch sites, scenes and missions…");
    search_->setClearButtonEnabled(true);
    layout->addWidget(search_);
    sites_ = new QTableWidget(0, 4);
    sites_->setObjectName("sceneEventSites");
    sites_->setHorizontalHeaderLabels({"Launch site", "Original scene", "Current scene", "Scope"});
    sites_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    sites_->setSelectionBehavior(QAbstractItemView::SelectRows);
    sites_->setSelectionMode(QAbstractItemView::SingleSelection);
    sites_->setAlternatingRowColors(true);
    sites_->verticalHeader()->hide();
    sites_->horizontalHeader()->setStretchLastSection(true);
    sites_->setColumnWidth(0, 225);
    sites_->setColumnWidth(1, 240);
    sites_->setColumnWidth(2, 240);
    layout->addWidget(sites_, 1);
    auto controls = new QHBoxLayout;
    target_ = new QComboBox;
    target_->setObjectName("sceneEventTarget");
    target_->setEditable(true);
    target_->setInsertPolicy(QComboBox::NoInsert);
    target_->completer()->setFilterMode(Qt::MatchContains);
    target_->setMinimumContentsLength(20);
    assign_ = new QPushButton("Assign scene");
    assign_->setObjectName("sceneEventAssign");
    restore_ = new QPushButton("Restore original launch");
    restore_->setObjectName("sceneEventRestore");
    controls->addWidget(target_, 1);
    controls->addWidget(assign_);
    controls->addWidget(restore_);
    layout->addLayout(controls);
    status_ = new QLabel;
    status_->setObjectName("sceneEventStatus");
    status_->setWordWrap(true);
    layout->addWidget(status_);
    connect(search_, &QLineEdit::textChanged, this, [this] {
        for (int row = 0; row < sites_->rowCount(); ++row) {
            QString text;
            for (int col = 0; col < 4; ++col)
                text += sites_->item(row, col)->text() + ' ';
            text += sites_->item(row, 0)->toolTip();
            sites_->setRowHidden(row, !text.contains(search_->text(), Qt::CaseInsensitive));
        }
    });
    connect(sites_, &QTableWidget::currentCellChanged, this, [this] { select(); });
    connect(assign_, &QPushButton::clicked, this, [this] { apply(false); });
    connect(restore_, &QPushButton::clicked, this, [this] { apply(true); });
    connect(undo_, &QUndoStack::indexChanged, this, [this] { refresh(); });
    try {
        project_->loadSceneEvents();
        refresh();
    } catch (const std::exception &e) {
        status_->setText(QString::fromUtf8(e.what()));
        assign_->setEnabled(false);
        restore_->setEnabled(false);
    }
}
void SceneEventEditor::refresh() {
    const ViewPosition keepView(this);
    if (!project_->sceneEvents())
        return;
    const QSignalBlocker blocker(sites_), targetBlocker(target_);
    const int selected = sites_->currentRow();
    QMap<int, QString> names;
    const auto &scenes = *project_->scenePrograms();
    for (const auto &id : scenes.scenes()) {
        const auto source = scenes.resource(id);
        for (const auto selector : source.selectors)
            names[selector] = QString("%1 · %2").arg(selector).arg(
                id.authoredId.isEmpty() ? source.name
                                        : scenes.authoredScenes()[id.authoredId].name);
    }
    target_->clear();
    for (auto it = names.cbegin(); it != names.cend(); ++it)
        target_->addItem(it.value(), it.key());
    sites_->setRowCount(SceneEvents::sites().size());
    int row = 0;
    for (const auto &site : SceneEvents::sites()) {
        const auto target = project_->sceneEvents()->target(site.id);
        const QStringList values{site.id, QString("%1 · %2").arg(site.selector).arg(site.name),
                                 names.value(target, QString::number(target)),
                                 site.shared ? "Shared" : site.mission};
        for (int col = 0; col < 4; ++col)
            sites_->setItem(row, col, new QTableWidgetItem(values[col]));
        sites_->item(row, 0)->setToolTip(site.role + " " + site.mission);
        sites_->item(row, 2)->setForeground(target == site.selector ? palette().text()
                                                                    : QBrush(QColor("#982018")));
        ++row;
    }
    sites_->setCurrentCell(selected < 0 ? 0 : selected, 0);
    // Preserve the current filter while project changes update names and bindings.
    const auto query = search_->text();
    search_->textChanged(query);
    select();
}
void SceneEventEditor::select() {
    const auto item = sites_->item(sites_->currentRow(), 0);
    assign_->setEnabled(item);
    restore_->setEnabled(item);
    if (!item || !project_->sceneEvents())
        return;
    const auto &site = SceneEvents::site(item->text());
    target_->setCurrentIndex(target_->findData(project_->sceneEvents()->target(site.id)));
    restore_->setEnabled(project_->sceneEvents()->target(site.id) != site.selector);
    status_->setText(site.role + (site.shared
                                      ? " · Shared: this changes all callers of this launch site."
                                      : QString{}));
}
void SceneEventEditor::apply(bool restore) {
    const auto item = sites_->item(sites_->currentRow(), 0);
    if (!item)
        return;
    try {
        auto next = *project_->sceneEvents();
        if (restore)
            next.restore(item->text());
        else {
            const auto index = target_->findText(target_->currentText(), Qt::MatchExactly);
            if (index < 0)
                throw Error("Choose a scene from the target list.");
            next.bind(item->text(), target_->itemData(index).toInt(), *project_->scenePrograms());
        }
        if (next.json() != project_->sceneEvents()->json()) {
            undo_->push(new EventCommand(project_, next.json()));
            if (changed_)
                changed_();
        }
    } catch (const std::exception &e) {
        status_->setText(QString::fromUtf8(e.what()));
    }
}
} // namespace lordly
