#include "app/scenario_event_editor.h"
#include "app/content_refresh.h"
#include "engine/event_script.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QUndoStack>
#include <QVBoxLayout>
namespace lordly {
namespace {
class FlagCommand final : public QUndoCommand {
  public:
    FlagCommand(Project *p, QJsonObject after, const QString &label)
        : QUndoCommand(label), p_(p), before_(p->sceneEvents()->json()), after_(after) {}
    void undo() override { p_->setSceneEvents(before_); }
    void redo() override { p_->setSceneEvents(after_); }
  private:
    Project *p_;
    QJsonObject before_, after_;
};
constexpr int offsetRole = Qt::UserRole, flagRole = Qt::UserRole + 1,
    checkRole = Qt::UserRole + 2, valueRole = Qt::UserRole + 3, targetRole = Qt::UserRole + 4;
QString position(int at) { return QString("@%1").arg(at, 4, 16, QChar('0')).toUpper(); }
}
ScenarioEventEditor::ScenarioEventEditor(Project *p, QUndoStack *undo, std::function<void()> changed, QWidget *parent)
    : QWidget(parent), project_(p), undo_(undo), changed_(std::move(changed)) {
    setObjectName("scenarioEvents");
    auto layout = new QVBoxLayout(this);
    auto help = new QLabel("Edit what the game sets or checks during this scenario. These are script instructions, not the current values in a saved game. "
        "Named flags have confirmed meanings; other flags keep their IDs. Branches may skip or repeat steps.");
    help->setWordWrap(true); layout->addWidget(help);
    rows_ = new QComboBox; rows_->setObjectName("scenarioEventRow");
    layout->addWidget(rows_);
    all_ = new QCheckBox("Show all commands, including read-only game instructions");
    all_->setObjectName("scenarioEventAll"); layout->addWidget(all_);
    steps_ = new QTreeWidget; steps_->setObjectName("scenarioEventSteps");
    steps_->setHeaderLabels({"Position", "Event / action"});
    steps_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    steps_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    steps_->setWordWrap(true); steps_->setTextElideMode(Qt::ElideNone);
    layout->addWidget(steps_, 1);
    detail_ = new QLabel; detail_->setWordWrap(true); detail_->setObjectName("scenarioEventDetail");
    layout->addWidget(detail_);
    auto form = new QFormLayout;
    flags_ = new QComboBox; flags_->setObjectName("scenarioEventFlag");
    flags_->setEditable(true); flags_->setInsertPolicy(QComboBox::NoInsert);
    flags_->completer()->setCompletionMode(QCompleter::PopupCompletion);
    flags_->completer()->setFilterMode(Qt::MatchContains);
    flags_->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    for (int flag = 0; flag < 256; ++flag) flags_->addItem(EventScript::flagName(flag), flag);
    action_ = new QComboBox; action_->setObjectName("scenarioEventAction");
    form->addRow("Flag", flags_); form->addRow("Action", action_); layout->addLayout(form);
    auto buttons = new QHBoxLayout;
    apply_ = new QPushButton("Apply step"); apply_->setObjectName("scenarioEventApply"); buttons->addWidget(apply_);
    jump_ = new QPushButton("Go to branch destination"); jump_->setObjectName("scenarioEventJump"); buttons->addWidget(jump_);
    buttons->addStretch();
    restore_ = new QPushButton("Restore loaded flag instructions"); restore_->setObjectName("scenarioEventRestore"); buttons->addWidget(restore_);
    layout->addLayout(buttons);
    notice_ = new QLabel; notice_->setWordWrap(true); notice_->setObjectName("scenarioEventNotice"); layout->addWidget(notice_);
    connect(rows_, &QComboBox::currentIndexChanged, this, [this] { if (!loading_) showRow(); });
    connect(all_, &QCheckBox::toggled, this, [this] { showRow(); });
    connect(steps_, &QTreeWidget::currentItemChanged, this, [this] { selectStep(); });
    connect(apply_, &QPushButton::clicked, this, &ScenarioEventEditor::apply);
    connect(jump_, &QPushButton::clicked, this, [this] {
        const auto item = steps_->currentItem(); if (!item) return;
        const int target = item->data(0, targetRole).toInt();
        all_->setChecked(true);
        for (int h = 0; h < steps_->topLevelItemCount(); ++h) {
            auto parent = steps_->topLevelItem(h);
            for (int n = 0; n < parent->childCount(); ++n)
                if (parent->child(n)->data(0, offsetRole).toInt() == target) {
                    parent->setExpanded(true); steps_->setCurrentItem(parent->child(n));
                    steps_->scrollToItem(parent->child(n)); return;
                }
        }
        notice_->setText("This branch destination is outside the decoded portion of this handler.");
    });
    connect(restore_, &QPushButton::clicked, this, [this] {
        if (rows_->currentIndex() < 0) return;
        auto next = *project_->sceneEvents(); next.restoreFlags(rows_->currentData().toInt());
        commit(next, "Restore scenario event flags");
    });
    watchContentChanges(undo_, this, [this] { if (isVisible()) refresh(); });
    selectStep();
}
void ScenarioEventEditor::selectScenario(int key) {
    const bool changed = key != scenario_; scenario_ = key;
    if (changed) { QSignalBlocker block(rows_); rows_->clear(); }
    if (changed && isVisible()) refresh();
}
void ScenarioEventEditor::showEvent(QShowEvent *event) { QWidget::showEvent(event); refresh(); }
void ScenarioEventEditor::refresh() {
    loading_ = true;
    const int previous = rows_->currentIndex() >= 0 ? rows_->currentData().toInt() : -1;
    rows_->clear(); notice_->clear();
    try {
        if (scenario_ >= 0) {
            project_->loadSceneEvents();
            for (int row : project_->sceneEvents()->scenarioRows(scenario_))
                rows_->addItem(QString("%1 · Event row %2").arg(row >= 71 && row <= 109 ? "Area investigation" : "Story mission").arg(row), row);
        }
        rows_->setCurrentIndex(qMax(0, rows_->findData(previous)));
    } catch (const std::exception &e) { notice_->setText(QString::fromUtf8(e.what())); }
    loading_ = false;
    showRow();
}
void ScenarioEventEditor::showRow() {
    const int previous = steps_->currentItem() ? steps_->currentItem()->data(0, offsetRole).toInt() : -1;
    const QSignalBlocker block(steps_);
    steps_->clear();
    restore_->setEnabled(rows_->currentIndex() >= 0);
    if (rows_->currentIndex() < 0) { detail_->setText("No mission event row is mapped to this scenario."); selectStep(); return; }
    try {
        const int row = rows_->currentData().toInt();
        const auto bytes = SceneEvents::flagRow(SceneEvents::retailRow(project_->rom(), row), row, project_->sceneEvents()->flagEdits());
        const auto handlers = EventScript::decode(bytes);
        QMap<int, QString> sceneNames;
        for (const auto &id : project_->scenePrograms()->scenes())
            for (int selector : project_->scenePrograms()->resource(id).selectors)
                sceneNames[selector] = project_->scenePrograms()->displayName(id);
        QMap<int, SceneEventSite> launches;
        for (const auto &site : SceneEvents::sites()) if (site.row == row) launches[site.offset] = site;
        int handlerNumber = 0;
        for (const auto &handler : handlers) {
            auto parent = new QTreeWidgetItem(steps_, {QString::number(++handlerNumber), EventScript::groupName(handler.group) + " · " + position(handler.entry)});
            parent->setData(0, offsetRole, -1); parent->setExpanded(true);
            for (const auto &step : handler.steps) {
                if (!all_->isChecked() && step.flag < 0 && !launches.contains(step.offset)) continue;
                QString text = step.text;
                if (launches.contains(step.offset)) {
                    const auto &site = launches[step.offset];
                    const int selector = project_->sceneEvents()->target(site.id);
                    text = QString("Play cutscene %1 · %2").arg(selector).arg(sceneNames.value(selector, site.name));
                }
                auto item = new QTreeWidgetItem(parent, {position(step.offset), text});
                item->setData(0, offsetRole, step.offset); item->setData(0, flagRole, step.flag);
                item->setData(0, checkRole, step.check); item->setData(0, valueRole, step.value);
                item->setData(0, targetRole, step.target);
                item->setToolTip(1, text + "\nOriginal instruction location: " + position(step.offset));
                if (step.offset == previous) steps_->setCurrentItem(item);
            }
            if (!parent->childCount()) delete parent;
        }
        notice_->setText("Flag edits keep branch destinations and all unrelated instructions intact. Other command types are read-only here.");
    } catch (const std::exception &e) { notice_->setText(QString::fromUtf8(e.what())); restore_->setEnabled(false); }
    selectStep();
}
void ScenarioEventEditor::selectStep() {
    auto item = steps_->currentItem();
    const bool editable = item && item->parent() && item->data(0, flagRole).toInt() >= 0;
    flags_->setEnabled(editable); action_->setEnabled(editable); apply_->setEnabled(editable);
    jump_->setEnabled(item && item->parent() && item->data(0, targetRole).toInt() >= 0);
    action_->clear();
    if (!editable) { detail_->setText(item ? item->text(1) + " — read-only" : "Select a flag instruction to edit it."); return; }
    const bool check = item->data(0, checkRole).toBool();
    flags_->setCurrentIndex(flags_->findData(item->data(0, flagRole)));
    action_->addItem(check ? "Check OFF — take the branch when OFF" : "Clear — turn OFF", false);
    action_->addItem(check ? "Check ON — take the branch when ON" : "Set — turn ON", true);
    action_->setCurrentIndex(action_->findData(item->data(0, valueRole)));
    detail_->setText(item->text(1) + (check ? "\nThe other result continues after this check. Use Go to branch destination to inspect its target." : "\nThis changes the flag when the game reaches this step."));
}
void ScenarioEventEditor::apply() {
    if (!apply_->isEnabled()) return;
    try {
        if (flags_->currentText() != flags_->itemText(flags_->currentIndex())) throw Error("Choose a listed flag before applying");
        auto next = *project_->sceneEvents();
        next.editFlag(rows_->currentData().toInt(), steps_->currentItem()->data(0, offsetRole).toInt(), flags_->currentData().toInt(), action_->currentData().toBool());
        commit(next, "Edit scenario event flag");
    } catch (const std::exception &e) { notice_->setText(QString::fromUtf8(e.what())); }
}
void ScenarioEventEditor::commit(const SceneEvents &next, const QString &label) {
    if (next.json() != project_->sceneEvents()->json()) undo_->push(new FlagCommand(project_, next.json(), label));
    refresh(); changed_();
}
} // namespace lordly
