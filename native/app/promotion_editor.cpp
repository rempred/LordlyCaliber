#include "app/promotion_editor.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
namespace lordly {
class PromotionCommand final : public QUndoCommand {
  public:
    PromotionCommand(Project *project, const QString &label, QJsonObject before, QJsonObject after,
                     std::function<void()> changed)
        : QUndoCommand(label), project_(project), before_(std::move(before)),
          after_(std::move(after)), changed_(std::move(changed)) {}
    void undo() override {
        project_->setPromotion(before_);
        changed_();
    }
    void redo() override {
        project_->setPromotion(after_);
        changed_();
    }

  private:
    Project *project_;
    QJsonObject before_, after_;
    std::function<void()> changed_;
};
PromotionEditor::PromotionEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                                 QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("promotionEditor");
    auto layout = new QVBoxLayout(this);
    notice_ = new QLabel;
    notice_->setWordWrap(true);
    layout->addWidget(notice_);
    auto form = new QFormLayout;
    layout->addLayout(form);
    families_ = new QComboBox;
    families_->setObjectName("classFamily");
    form->addRow("Class family", families_);
    const QStringList labels = {"Strength", "Vitality",  "Intelligence",      "Mentality",
                                "Agility",  "Dexterity", "Alignment minimum", "Alignment maximum"};
    for (int n = 0; n < labels.size(); ++n) {
        auto spin = new QSpinBox;
        spin->setRange(0, 255);
        spin->setKeyboardTracking(false);
        spin->setObjectName(QString("promotionGate%1").arg(n));
        if (n < 6)
            spin->setSpecialValueText("No requirement");
        gates_ << spin;
        form->addRow(labels[n], spin);
        connect(spin, &QSpinBox::valueChanged, this, [this, n](int v) {
            edit("Edit promotion requirement", [=, this](Promotion &p) { p.setGate(id_, n, v); });
        });
    }
    mask_ = new QSpinBox;
    mask_->setRange(0, 255);
    mask_->setDisplayIntegerBase(16);
    mask_->setPrefix("0x");
    mask_->setKeyboardTracking(false);
    mask_->setObjectName("compatibilityMask");
    mask_->setToolTip("Advanced category bits used by roster grouping, discovery and class-change "
                      "compatibility. A shared bit does not by itself permit promotion.");
    form->addRow("Compatibility mask", mask_);
    connect(mask_, &QSpinBox::valueChanged, this, [this](int v) {
        edit("Edit compatibility mask", [=, this](Promotion &p) { p.setMask(id_, v); });
    });
    equipmentSets_ = new QCheckBox("Available after discovery");
    equipmentSets_->setObjectName("standardEquipmentList");
    equipmentSets_->setToolTip("Include this class's standard equipment set in the Army purchase "
                               "list. Normal Goth and inventory checks apply.");
    form->addRow("Standard equipment set", equipmentSets_);
    connect(equipmentSets_, &QCheckBox::toggled, this, [this](bool listed) {
        edit("Edit standard-equipment availability",
             [=, this](Promotion &p) { p.setEquipmentSetListed(id_, listed); });
    });
    connect(families_, &QComboBox::currentIndexChanged, this, [this] {
        edit("Assign class family",
             [this](Promotion &p) { p.assign(id_, families_->currentData().toInt()); });
    });
    layout->addSpacing(18);
    auto heading = new QLabel("Create a family");
    layout->addWidget(heading);
    auto familyForm = new QFormLayout;
    layout->addLayout(familyForm);
    name_ = new QLineEdit;
    name_->setPlaceholderText("Family name");
    name_->setMaxLength(80);
    name_->setObjectName("newFamilyName");
    templates_ = new QComboBox;
    templates_->setObjectName("familyTemplate");
    familyForm->addRow("Name", name_);
    familyForm->addRow("Behavior template", templates_);
    auto add = new QPushButton("Create and assign family");
    add->setObjectName("createFamily");
    layout->addWidget(add);
    connect(add, &QPushButton::clicked, this, [this] {
        edit("Create class family", [this](Promotion &p) {
            p.assign(id_, p.addFamily(name_->text(), templates_->currentData().toInt()));
        });
    });
    auto detail =
        new QLabel("A new family has its own membership and inherits the selected family's game "
                   "rules. Stat and alignment requirements remain editable above.");
    detail->setWordWrap(true);
    layout->addWidget(detail);
    layout->addStretch();
    refresh();
}
void PromotionEditor::selectClass(int id) {
    id_ = id;
    refresh();
}
void PromotionEditor::refresh() {
    refreshing_ = true;
    const bool available = project_->promotion().has_value();
    const bool editable = available && id_ >= 1 && id_ < project_->promotion()->classCount();
    families_->setEnabled(editable);
    mask_->setEnabled(editable);
    equipmentSets_->setEnabled(editable);
    name_->setEnabled(editable);
    templates_->setEnabled(editable);
    findChild<QPushButton *>("createFamily")->setEnabled(editable);
    for (auto spin : gates_)
        spin->setEnabled(editable);
    notice_->setText(!available  ? "Promotion source owners are available for US Rev 0."
                     : !editable ? "This class is outside the existing manual-promotion roster."
                                 : "Manual class-change requirements. Equipment, story and "
                                   "character conditions also apply.");
    families_->clear();
    auto templateId = templates_->currentData().toInt();
    templates_->clear();
    if (available) {
        const auto &p = *project_->promotion();
        for (auto f : p.families["families"].toArray()) {
            auto o = f.toObject();
            auto id = o["id"].toInt();
            auto label = QString("%1 · %2").arg(id).arg(o["name"].toString());
            families_->addItem(label, id);
            if (o["behavior"].toInt() > 0)
                templates_->addItem(label, id);
        }
        templates_->setCurrentIndex(std::max(0, templates_->findData(templateId)));
        if (editable) {
            families_->setCurrentIndex(
                families_->findData(p.families["assignments"].toArray()[id_].toInt()));
            for (int n = 0; n < gates_.size(); ++n)
                gates_[n]->setValue(u8(p.gates, id_ * 8 + n));
            mask_->setValue(u8(p.masks, id_));
            equipmentSets_->setChecked((u8(p.flags, id_) & 0x80) != 0);
        }
    }
    refreshing_ = false;
}
void PromotionEditor::edit(const QString &label,
                           const std::function<void(Promotion &)> &operation) {
    if (refreshing_ || !project_->promotion() || id_ < 1 ||
        id_ >= project_->promotion()->classCount())
        return;
    try {
        auto before = project_->promotion()->json();
        auto candidate = *project_->promotion();
        operation(candidate);
        auto after = candidate.json();
        if (before != after) {
            QPointer<PromotionEditor> self(this);
            undo_->push(new PromotionCommand(project_, label, before, after, [self] {
                if (self) {
                    self->refresh();
                    self->changed_();
                }
            }));
        }
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
} // namespace lordly
