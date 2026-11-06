#include "app/content_refresh.h"
#include "app/consumable_editor.h"
#include "app/pending_edits.h"
#include "app/consumable_tree_dialog.h"
#include "app/consumable_visual_dialog.h"
#include "engine/consumable_tree.h"
#include "engine/consumable_visual.h"
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QHBoxLayout>
#include <QScopedValueRollback>
#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
namespace lordly {
class EffectCommand final : public QUndoCommand {
  public:
    EffectCommand(Project *project, Project before, Project after)
        : QUndoCommand("Edit consumable effect"), project_(project), before_(std::move(before)),
          after_(std::move(after)) {}
    void undo() override {
        *project_ = before_;
    }
    void redo() override {
        *project_ = after_;
    }

  private:
    Project *project_;
    Project before_, after_;
};
ConsumableEditor::ConsumableEditor(Project *project, QUndoStack *undo,
                                   std::function<void()> changed, QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("consumableEffectEditor");
    auto layout = new QVBoxLayout(this);
    heading_ = new QLabel;
    target_ = new QLabel;
    heading_->setWordWrap(true);
    target_->setWordWrap(true);
    layout->addWidget(heading_);
    layout->addWidget(target_);
    tree_ = createConsumableTreeView([this](QTreeWidgetItem *effect, QTreeWidgetItem *branch) {
        return moveEffect(effect, branch);
    });
    tree_->setObjectName("consumableTreeOverview");
    tree_->setHeaderHidden(true);
    tree_->setMinimumHeight(180);
    layout->addWidget(tree_, 1);
    auto treeButtons=new QHBoxLayout;editTree_=new QPushButton("Edit effect tree…");editTree_->setObjectName("editConsumableTree");restoreTree_=new QPushButton("Restore loaded tree");restoreTree_->setObjectName("restoreConsumableTree");treeButtons->addWidget(editTree_);treeButtons->addWidget(restoreTree_);layout->addLayout(treeButtons);
    connect(editTree_, &QPushButton::clicked, this, [this] {
        auto *row = tree_->currentItem();
        openTree(row ? row->data(0, Qt::UserRole).value<QList<int>>() : QList<int>{});
    });
    connect(tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *row) {
        const auto path = row->data(0, Qt::UserRole);
        if (path.isValid())
            openTree(path.value<QList<int>>());
    });
    connect(restoreTree_,&QPushButton::clicked,this,[this]{
        auto effects=*project_->consumableEffects();const auto loaded=ConsumableEffects::load(project_->rom());effects.setTree(selected_,loaded.tree(selected_));auto next=*project_;next.setConsumableEffects(effects.json());
        if(next.manifest()!=project_->manifest())undo_->push(new EffectCommand(project_,*project_,std::move(next)));
    });
    visualName_ = new QLabel; visualName_->setWordWrap(true); layout->addWidget(visualName_);
    auto visualButtons = new QHBoxLayout; layout->addLayout(visualButtons);
    editVisual_ = new QPushButton("Edit use animation…"); editVisual_->setObjectName("editConsumableVisual");
    restoreVisual_ = new QPushButton("Restore loaded animation"); restoreVisual_->setObjectName("restoreConsumableVisual");
    visualButtons->addWidget(editVisual_); visualButtons->addWidget(restoreVisual_);
    connect(editVisual_, &QPushButton::clicked, this, [this] {
        try {
            if (!applyDrafts()) return;
            auto effects = *project_->consumableEffects();
            const auto key = QString::number(selected_);
            const auto before = effects.visuals().contains(key) ? effects.visuals()[key].toObject()
                : ConsumableVisual::retail(project_->runtimeRom(), selected_).json();
            if (const auto visual = editConsumableVisual(this, *project_, selected_); visual && *visual != before) {
                effects.setVisual(selected_, *visual); auto next = *project_; next.setConsumableEffects(effects.json());
                undo_->push(new EffectCommand(project_, *project_, std::move(next)));
            }
        } catch (const std::exception &e) { notice_->setText(QString::fromUtf8(e.what())); }
    });
    connect(restoreVisual_, &QPushButton::clicked, this, [this] {
        try {
            auto effects = *project_->consumableEffects(); const auto key = QString::number(selected_);
            const auto loaded = ConsumableEffects::load(project_->rom());
            if (loaded.visuals().contains(key)) effects.setVisual(selected_, loaded.visuals()[key].toObject());
            else effects.restoreVisual(selected_);
            auto next = *project_; next.setConsumableEffects(effects.json());
            if (next.manifest() != project_->manifest()) undo_->push(new EffectCommand(project_, *project_, std::move(next)));
        } catch (const std::exception &e) { notice_->setText(QString::fromUtf8(e.what())); }
    });
    originalSettings_ = new QGroupBox("Original ROM values");
    auto originalLayout = new QVBoxLayout(originalSettings_);
    layout->addWidget(originalSettings_);
    auto form = new QFormLayout;
    minimumLabel_ = new QLabel;
    maximumLabel_ = new QLabel("Maximum");
    minimum_ = new QSpinBox;
    maximum_ = new QSpinBox;
    minimum_->setObjectName("effectMinimum");
    maximum_->setObjectName("effectMaximum");
    form->addRow(minimumLabel_, minimum_);
    form->addRow(maximumLabel_, maximum_);
    originalLayout->addLayout(form);
    description_ = new QCheckBox("Update the HP amount in the description");
    description_->setObjectName("effectUpdateDescription");
    description_->setChecked(true);
    originalLayout->addWidget(description_);
    apply_ = new QPushButton("Apply effect");
    restore_ = new QPushButton("Restore loaded effect");
    apply_->setObjectName("effectApply");
    restore_->setObjectName("effectRestore");
    discard_ = new QPushButton("Discard pending effect");
    discard_->setObjectName("effectDiscard");
    originalLayout->addWidget(apply_);
    originalLayout->addWidget(discard_);
    originalLayout->addWidget(restore_);
    originalNote_ = new QLabel;
    originalNote_->setWordWrap(true);
    originalLayout->addWidget(originalNote_);
    notice_ = new QLabel;
    notice_->setObjectName("effectNotice");
    notice_->setWordWrap(true);
    layout->addWidget(notice_);
    layout->addStretch();
    pending_ = new PendingEdits(this);
    pending_->entries = [this] {
        QVector<PendingEdit> entries;
        for (const auto &draft : drafts_)
            entries.append({"Consumable effect · " + project_->record(Table::Consumables, draft.id).name.simplified(),
                            {"consumables", draft.id}});
        return entries;
    };
    pending_->apply = [this] { return applyDrafts(); };
    pending_->discard = [this] { drafts_.clear(); refresh(); pending_->notify(); };
    for (auto spin : {minimum_, maximum_}) {
        spin->setKeyboardTracking(false);
        connect(spin, &QSpinBox::valueChanged, this, [this] { rememberDraft(); });
    }
    connect(description_, &QCheckBox::toggled, this, [this] { rememberDraft(); });
    connect(discard_, &QPushButton::clicked, this, [this] {
        drafts_.remove(definition_["key"].toString()); refresh(); pending_->notify();
    });
    connect(apply_, &QPushButton::clicked, this, [this] { apply(false); });
    connect(restore_, &QPushButton::clicked, this, [this] { apply(true); });
    watchContentChanges(undo_, this, [this] {
        refresh();
        changed_();
    });
    refresh();
}
void ConsumableEditor::selectConsumable(int id) {
    selected_ = id;
    refresh();
}
void ConsumableEditor::setAdvancedVisible(bool visible) {
    advanced_ = visible;
    refresh();
}
void ConsumableEditor::openTree(const QList<int> &path) {
    if (!applyDrafts())
        return;
    const auto tree = editConsumableTree(this, *project_, selected_, path);
    if (!tree)
        return;
    commitTree(*tree);
}
void ConsumableEditor::commitTree(const QJsonArray &tree) {
    auto effects = *project_->consumableEffects();
    effects.setTree(selected_, tree);
    if (effects.json() != project_->consumableEffects()->json()) {
        auto next = *project_;
        next.setConsumableEffects(effects.json());
        undo_->push(new EffectCommand(project_, *project_, std::move(next)));
    }
}
bool ConsumableEditor::moveEffect(QTreeWidgetItem *effect, QTreeWidgetItem *branch) {
    if (!drafts_.isEmpty()) {
        // Applying a pending ROM amount rebuilds this view. Reacquire both rows
        // so the move carries the newly applied amount and never uses stale items.
        const auto from = effect->data(0, Qt::UserRole);
        const auto to = branch->data(0, ConsumableBranchRole);
        if (!applyDrafts()) return false;
        effect = nullptr;
        branch = nullptr;
        for (QTreeWidgetItemIterator it(tree_); *it; ++it) {
            if ((*it)->data(0, Qt::UserRole) == from) effect = *it;
            if ((*it)->data(0, ConsumableBranchRole) == to) branch = *it;
        }
        if (!effect || !branch) return false;
    }
    try {
        moveConsumableRow(effect, branch);
        const auto tree = consumableSteps(tree_->topLevelItem(0));
        if (tree == project_->consumableEffects()->tree(selected_))
            refresh();
        else
            commitTree(tree);
        return true;
    } catch (const std::exception &e) {
        refresh();
        notice_->setText(QString::fromUtf8(e.what()));
        return false;
    }
}
void ConsumableEditor::refresh() {
    const QScopedValueRollback<bool> guard(refreshing_, true);
    definition_ = {};
    for (const auto &entry : ConsumableEffects::definitions()) {
        const auto def = entry.toObject();
        if (def["itemIds"].toArray().contains(selected_)) {
            definition_ = def;
            break;
        }
    }
    const bool custom=project_->consumableEffects()->trees().contains(QString::number(selected_));
    const bool visualSupported = ConsumableTree::supported(selected_) || selected_ == 20;
    editVisual_->setVisible(visualSupported); restoreVisual_->setVisible(visualSupported);
    const auto visuals = project_->consumableEffects()->visuals();
    visualName_->setText(visualSupported ? "Use animation · " + (visuals.contains(QString::number(selected_))
        ? visuals[QString::number(selected_)].toObject()["name"].toString() : "Original sparkle effect")
        : selected_ >= 39 ? "Combat visuals use the assigned attack effect." : QString());
    const bool supported = !definition_.isEmpty()&&!custom;
    originalSettings_->setVisible(advanced_ && supported);
    editTree_->setEnabled(ConsumableTree::supported(selected_));restoreTree_->setEnabled(ConsumableTree::supported(selected_));tree_->clear();
    QString trigger = "WHEN: Item used";
    if (selected_ >= 24 && selected_ <= 31) trigger = "WHILE: Carried by the squad";
    else if (selected_ >= 32 && selected_ <= 38) trigger = "WHEN: Referenced by a story event";
    else if (selected_ >= 39) trigger = "WHEN: Pedra invoked in battle";
    else if (selected_ == 8 || (selected_ >= 21 && selected_ <= 23)) trigger = "WHEN: Used on the battlefield map";
    else if (selected_ <= 0) trigger = "No consumable selected";
    auto when = new QTreeWidgetItem(tree_, {trigger});
    if(ConsumableTree::supported(selected_)){
        when->setData(0, ConsumableBranchRole, QVariant::fromValue(QList<int>{}));
        new QTreeWidgetItem(when,{u8(project_->record(Table::Consumables,selected_).bytes,8)==2?"TARGET: Squad members":"TARGET: Selected character"});
        std::function<void(QTreeWidgetItem *, const QJsonArray &, QList<int>)> add =
            [&](QTreeWidgetItem *parent, const QJsonArray &nodes, QList<int> parentPath) {
                for (int i = 0; i < nodes.size(); ++i) {
                    const auto n = nodes.at(i).toObject();
                    const auto path = parentPath + QList<int>{i};
                    const auto text = consumableStepLabel(*project_, n);
                    auto row = new QTreeWidgetItem(parent, {text});
                    row->setData(0, Qt::UserRole, QVariant::fromValue(path));
                    row->setData(0, ConsumableNodeRole, n);
                    row->setForeground(0, palette().link());
                    row->setToolTip(0, text);
                    if (n["type"] == "if") {
                        auto then = new QTreeWidgetItem(row, {"THEN"});
                        auto otherwise = new QTreeWidgetItem(row, {"ELSE"});
                        const auto thenPath = path + QList<int>{0};
                        const auto elsePath = path + QList<int>{1};
                        then->setData(0, ConsumableBranchRole, QVariant::fromValue(thenPath));
                        otherwise->setData(0, ConsumableBranchRole, QVariant::fromValue(elsePath));
                        add(then, n["then"].toArray(), thenPath);
                        add(otherwise, n["else"].toArray(), elsePath);
                    }
                }
            };
        add(when,project_->consumableEffects()->tree(selected_),{});new QTreeWidgetItem(when,{"CONSUME: One item after confirmation"});
    }else{
        static const QMap<int,QString> native={{8,"Return the squad to base"},{20,"Raise squad Unity by a random 1–5, up to 100"},{21,"Use the Silver Hourglass map effect"},{22,"Discover a remaining map treasure"},{23,"Use Love and Peace recruitment"},{24,"Increase Soldier progress"},{25,"Change the Soldier graduation choice"},{26,"Reduce marching fatigue accumulation"},{27,"Increase recovery rate"},{28,"Apply the Figurine's extra map update"},{29,"Increase battle experience"},{30,"Protect against Zombie conversion"},{31,"Enable the Charge Horn's combat command uses"}};
        if (selected_ > 0)
            new QTreeWidgetItem(when,{native.value(selected_,selected_>=39?"Invoke the assigned Pedra attack":"Used by story events")});
    }
    tree_->expandAll();
    heading_->setText(selected_ >= 0 ? project_->record(Table::Consumables, selected_).name
                                     : "Select a consumable");
    static const QMap<QString, QString> targets = {
        {"healLeaf", "Restores current HP to one target, up to its current limit."},
        {"healSeed", "Restores current HP to one target, up to its current limit."},
        {"healPack", "Restores current HP across the unit, up to each target's current limit."},
        {"powerFruit",
         "Reduces a character's fatigue, down to zero. Special targets are unchanged."},
        {"angelFruit",
         "Reduces a character's fatigue, down to zero. Special targets are unchanged."},
        {"cupOfLife", "Adds a random amount to maximum HP, capped at 999."},
        {"sharedStatBoosters11To16", "Adds a random amount to the item's stat, capped at 999."},
        {"scrollOfDiscipline", "Raises alignment by a random amount, capped at 100."},
        {"urnOfChaos", "Lowers alignment by a random amount, down to zero."},
        {"gobletOfDestiny", "Changes luck by a random amount, within 0–100."}};
    target_->setText(custom?"Custom effect tree":ConsumableTree::supported(selected_)?QString():"Original game behavior");
    minimum_->setVisible(supported);
    minimumLabel_->setVisible(supported);
    const bool range = supported && definition_["kind"] == "range";
    maximum_->setVisible(range);
    maximumLabel_->setVisible(range);
    description_->setVisible(supported && selected_ >= 1 && selected_ <= 3);
    apply_->setVisible(supported);restore_->setVisible(supported);
    apply_->setEnabled(supported);
    restore_->setEnabled(supported);
    notice_->clear();
    discard_->hide();
    if (!supported)
        return;
    const auto values =
        project_->consumableEffects()->models()[definition_["key"].toString()].toObject();
    minimumLabel_->setText(range ? "Minimum" : "Amount");
    for (auto spin : {minimum_, maximum_})
        spin->setRange(definition_["domainMin"].toInt(), definition_["domainMax"].toInt());
    minimum_->setValue(values[range ? "minimum" : "magnitude"].toInt());
    maximum_->setValue(values["maximum"].toInt());
    originalNote_->clear();
    if (selected_ >= 11 && selected_ <= 16)
        originalNote_->setText("This range is shared by all six stat boosters (items 11–16).");
    else if (selected_ >= 1 && selected_ <= 3)
        originalNote_->setText("Zero consumes the item without healing. The description update changes "
                         "only one recognizable HP amount; custom wording is preserved.");
    const auto key = definition_["key"].toString();
    if (drafts_.contains(key)) {
        const auto &draft = drafts_[key];
        minimum_->setValue(draft.values[range ? "minimum" : "magnitude"].toInt());
        maximum_->setValue(draft.values["maximum"].toInt());
        description_->setChecked(draft.description);
        discard_->show();
        notice_->setText(draft.source == values
            ? "Pending effect changes — Apply or Discard. Switching items retains this draft."
            : "The saved effect changed while this draft was pending. Discard this draft and edit the current values, or undo that other change first.");
    }
}
void ConsumableEditor::rememberDraft() {
    if (refreshing_ || definition_.isEmpty()) return;
    const auto key = definition_["key"].toString();
    const auto source = project_->consumableEffects()->models()[key].toObject();
    const QJsonObject values = definition_["kind"] == "range"
        ? QJsonObject{{"minimum", minimum_->value()}, {"maximum", maximum_->value()}}
        : QJsonObject{{"magnitude", minimum_->value()}};
    if (values == source) drafts_.remove(key);
    else drafts_[key] = {selected_, drafts_.contains(key) ? drafts_[key].source : source,
                         values, description_->isChecked()};
    discard_->setVisible(drafts_.contains(key));
    if (drafts_.contains(key)) notice_->setText("Pending effect changes — Apply or Discard. Switching items retains this draft.");
    else notice_->clear();
    pending_->notify();
}
bool ConsumableEditor::applyDrafts() {
    if (drafts_.isEmpty())
        return true;
    try {
        auto next = *project_;
        for (auto it = drafts_.cbegin(); it != drafts_.cend(); ++it) {
            if (it->source != project_->consumableEffects()->models()[it.key()].toObject())
                throw Error("A pending effect conflicts with another edit. Review it before applying.");
            next.editConsumableEffect(it.key(), it->values, it->description);
        }
        drafts_.clear();
        if (next.manifest() != project_->manifest())
            undo_->push(new EffectCommand(project_, *project_, std::move(next)));
        refresh(); pending_->notify(); return true;
    } catch (const std::exception &error) {
        notice_->setText(QString::fromUtf8(error.what())); return false;
    }
}
void ConsumableEditor::apply(bool restore) {
    if (definition_.isEmpty())
        return;
    try {
        const auto key = definition_["key"].toString();
        if (!restore && drafts_.contains(key) && drafts_[key].source != project_->consumableEffects()->models()[key].toObject())
            throw Error("The saved effect changed. Resolve the pending draft before applying.");
        QJsonObject values =
            definition_["kind"] == "range"
                ? QJsonObject{{"minimum", minimum_->value()}, {"maximum", maximum_->value()}}
                : QJsonObject{{"magnitude", minimum_->value()}};
        if (restore)
            values = ConsumableEffects::load(project_->rom()).models()[key].toObject();
        auto next = *project_;
        next.editConsumableEffect(key, values, description_->isChecked());
        drafts_.remove(key);
        if (next.manifest() != project_->manifest())
            undo_->push(new EffectCommand(project_, *project_, std::move(next)));
        else
            refresh();
        pending_->notify();
    } catch (const std::exception &error) {
        notice_->setText(QString::fromUtf8(error.what()));
    }
}
} // namespace lordly
