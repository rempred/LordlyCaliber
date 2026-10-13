#include "app/content_refresh.h"
#include "app/consumable_editor.h"
#include <QCheckBox>
#include <QFormLayout>
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
    auto form = new QFormLayout;
    minimumLabel_ = new QLabel;
    maximumLabel_ = new QLabel("Maximum");
    minimum_ = new QSpinBox;
    maximum_ = new QSpinBox;
    minimum_->setObjectName("effectMinimum");
    maximum_->setObjectName("effectMaximum");
    form->addRow(minimumLabel_, minimum_);
    form->addRow(maximumLabel_, maximum_);
    layout->addLayout(form);
    description_ = new QCheckBox("Update the HP amount in the description");
    description_->setObjectName("effectUpdateDescription");
    description_->setChecked(true);
    layout->addWidget(description_);
    apply_ = new QPushButton("Apply effect");
    restore_ = new QPushButton("Restore loaded effect");
    apply_->setObjectName("effectApply");
    restore_->setObjectName("effectRestore");
    layout->addWidget(apply_);
    layout->addWidget(restore_);
    notice_ = new QLabel;
    notice_->setObjectName("effectNotice");
    notice_->setWordWrap(true);
    layout->addWidget(notice_);
    layout->addStretch();
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
void ConsumableEditor::refresh() {
    definition_ = {};
    for (const auto &entry : ConsumableEffects::definitions()) {
        const auto def = entry.toObject();
        if (def["itemIds"].toArray().contains(selected_)) {
            definition_ = def;
            break;
        }
    }
    const bool supported = !definition_.isEmpty();
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
    target_->setText(supported ? targets[definition_["key"].toString()]
                               : "This item's behavior has no editable effect model yet.");
    minimum_->setVisible(supported);
    minimumLabel_->setVisible(supported);
    const bool range = supported && definition_["kind"] == "range";
    maximum_->setVisible(range);
    maximumLabel_->setVisible(range);
    description_->setVisible(selected_ >= 1 && selected_ <= 3);
    apply_->setEnabled(supported);
    restore_->setEnabled(supported);
    notice_->clear();
    if (!supported)
        return;
    const auto values =
        project_->consumableEffects()->models()[definition_["key"].toString()].toObject();
    minimumLabel_->setText(range ? "Minimum" : "Amount");
    for (auto spin : {minimum_, maximum_})
        spin->setRange(definition_["domainMin"].toInt(), definition_["domainMax"].toInt());
    minimum_->setValue(values[range ? "minimum" : "magnitude"].toInt());
    maximum_->setValue(values["maximum"].toInt());
    if (selected_ >= 11 && selected_ <= 16)
        notice_->setText("This range is shared by all six stat boosters (items 11–16).");
    else if (selected_ >= 1 && selected_ <= 3)
        notice_->setText("Zero consumes the item without healing. The description update changes "
                         "only one recognizable HP amount; custom wording is preserved.");
}
void ConsumableEditor::apply(bool restore) {
    if (definition_.isEmpty())
        return;
    try {
        const auto key = definition_["key"].toString();
        QJsonObject values =
            definition_["kind"] == "range"
                ? QJsonObject{{"minimum", minimum_->value()}, {"maximum", maximum_->value()}}
                : QJsonObject{{"magnitude", minimum_->value()}};
        if (restore)
            values = ConsumableEffects::load(project_->rom()).models()[key].toObject();
        auto next = *project_;
        next.editConsumableEffect(key, values, description_->isChecked());
        if (next.manifest() != project_->manifest())
            undo_->push(new EffectCommand(project_, *project_, std::move(next)));
        else
            refresh();
    } catch (const std::exception &error) {
        notice_->setText(QString::fromUtf8(error.what()));
    }
}
} // namespace lordly
