#include "app/content_refresh.h"
#include "app/promotion_editor.h"
#include <QDialog>
#include <QDialogButtonBox>
#include "app/view_position.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
namespace lordly {
namespace {
const QStringList compatibilityNames{"Male", "Female", "Angels / undead", "Hawkmen / other small creatures",
                                     "Dragons", "Beasts", "Golems", "Goblins / ogres / Gorgon"};
QString compatibilityName(int mask) {
    QStringList names;
    for (int bit=0;bit<compatibilityNames.size();++bit)
        if (mask & (1<<bit)) names << compatibilityNames[bit];
    return names.isEmpty() ? QString("None") : names.join(" + ");
}
}
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
class FamilyIconCommand final : public QUndoCommand {
  public:
    FamilyIconCommand(Project *p, QJsonObject after)
        : QUndoCommand("Assign family tab icon"), p_(p), before_(p->categoryIcons()->json()),
          after_(after) {}
    void undo() override {
        p_->setCategoryIcons(before_);
    }
    void redo() override {
        p_->setCategoryIcons(after_);
    }

  private:
    Project *p_;
    QJsonObject before_, after_;
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
    auto iconRow = new QWidget;
    auto iconLayout = new QHBoxLayout(iconRow);
    iconLayout->setContentsMargins(0, 0, 0, 0);
    familyIconPreview_ = new QLabel;
    familyIconPreview_->setFixedSize(42, 42);
    iconLayout->addWidget(familyIconPreview_);
    familyIcon_ = new QComboBox;
    familyIcon_->setObjectName("familyTabIcon");
    familyIcon_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    familyIcon_->setMinimumContentsLength(10);
    iconLayout->addWidget(familyIcon_, 1);
    form->addRow("List Classes icon", iconRow);
    auto iconActions = new QHBoxLayout;
    auto editIcon = new QPushButton("Edit icon…");
    editIcon->setObjectName("editFamilyIcon");
    iconActions->addWidget(editIcon);
    auto createIcon = new QPushButton("Create icon…");
    createIcon->setObjectName("createFamilyIcon");
    iconActions->addWidget(createIcon);
    form->addRow(iconActions);
    auto iconHelp = new QLabel(
        "Choose an icon to give this family its own List Classes tab. Classes still follow the "
        "game's discovery rules. Shared icons change for every family using them.");
    iconHelp->setWordWrap(true);
    form->addRow(iconHelp);
    connect(editIcon, &QPushButton::clicked, this, [this] {
        emit editFamilyIconRequested(families_->currentData().toInt(),
                                     familyIcon_->currentData().toInt(), false);
    });
    connect(createIcon, &QPushButton::clicked, this, [this] {
        emit editFamilyIconRequested(families_->currentData().toInt(),
                                     familyIcon_->currentData().toInt(), true);
    });
    connect(familyIcon_, &QComboBox::currentIndexChanged, this, [this] {
        if (refreshing_ || !project_->categoryIcons() || families_->currentData().toInt() < 1)
            return;
        auto edited = *project_->categoryIcons();
        try {
            edited.assignFamily(families_->currentData().toInt(),
                                familyIcon_->currentData().toInt());
            if (edited.json() != project_->categoryIcons()->json())
                undo_->push(new FamilyIconCommand(project_, edited.json()));
            refresh();
            changed_();
        } catch (const std::exception &e) {
            refresh();
            notice_->setText(QString::fromUtf8(e.what()));
        }
    });
    watchContentChanges(undo_, this, [this] { refresh(); });
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
    mask_ = new QComboBox;
    mask_->setMinimumContentsLength(18);
    mask_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    mask_->setObjectName("compatibilityMask");
    mask_->setToolTip("Existing List Classes categories and class-change compatibility. "
                      "A matching category does not by itself permit a class change.");
    form->addRow("Compatibility category", mask_);
    auto categoryHelp = new QLabel("Separate from Class family above. New classes start with family None and category Male. "
        "A family with its own icon uses its own List Classes tab.");
    categoryHelp->setWordWrap(true);form->addRow(categoryHelp);
    connect(mask_, &QComboBox::currentIndexChanged, this, [this] {
        if (refreshing_ || mask_->currentIndex()<0) return;
        int value=mask_->currentData().toInt();
        if (value<0) {
            QDialog dialog(this);dialog.setObjectName("compatibilityCategoryDialog");
            dialog.setWindowTitle("Choose compatibility categories");
            auto layout=new QVBoxLayout(&dialog);
            QVector<QCheckBox *> bits;
            const int current=u8(project_->promotion()->masks,id_);
            for (int bit=0;bit<compatibilityNames.size();++bit) {
                auto check=new QCheckBox(compatibilityNames[bit]);
                check->setObjectName(QString("compatibilityBit_%1").arg(1<<bit));
                check->setChecked(current & (1<<bit));bits << check;layout->addWidget(check);
            }
            auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
            layout->addWidget(buttons);
            connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
            connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
            if (dialog.exec()!=QDialog::Accepted) {refresh();return;}
            value=0;for (int bit=0;bit<bits.size();++bit) if(bits[bit]->isChecked())value|=1<<bit;
        }
        edit("Edit compatibility category", [=, this](Promotion &p) { p.setMask(id_, value); });
        refresh();
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
    layout->addSpacing(12);
    linksTitle_ = new QLabel;
    linksTitle_->setWordWrap(true);
    layout->addWidget(linksTitle_);
    links_ = new QListWidget;
    links_->setObjectName("familyLinks");
    links_->setMaximumHeight(170);
    layout->addWidget(links_);
    auto applyLinks = new QPushButton("Apply family links");
    applyLinks->setObjectName("applyFamilyLinks");
    layout->addWidget(applyLinks);
    connect(applyLinks, &QPushButton::clicked, this, [this] {
        QSet<int> links;
        for (int row = 0; row < links_->count(); ++row)
            if (links_->item(row)->checkState() == Qt::Checked)
                links.insert(links_->item(row)->data(Qt::UserRole).toInt());
        edit("Link class-change families", [this, links](Promotion &p) {
            p.setFamilyLinks(families_->currentData().toInt(), links);
        });
    });
    auto linkHelp =
        new QLabel("Links work in both directions, including through other linked families. They "
                   "add destinations to the manual class-change menu; compatibility masks, each "
                   "family's manual-change policy, discovery, stats and equipment still apply. "
                   "Family None cannot be linked.");
    linkHelp->setWordWrap(true);
    layout->addWidget(linkHelp);
    auto automatic = new QLabel(
        "Automatic changes are separate from families. Angel Knight / Zombie creation uses a "
        "dedicated scenario-end function and per-class death-eligibility flags. Fallen non-leader "
        "women may become Angel Knights (INT 60, MEN 61, DEX 57, Alignment 50–100, spare gear and "
        "a Luck roll); eligible fallen characters may become Zombies unless protected by Mastaba's "
        "Barrier. Zombie → Skeleton and Skeleton → Ghost use attack-conversion rules. Linking a "
        "family does not enable these automatic routes.");
    automatic->setWordWrap(true);
    layout->addWidget(automatic);
    layout->addSpacing(18);
    auto heading = new QLabel("Create a family");
    layout->addWidget(heading);
    auto familyForm = new QFormLayout;
    layout->addLayout(familyForm);
    name_ = new QLineEdit;
    name_->setPlaceholderText("Family name");
    name_->setMaxLength(80);
    name_->setObjectName("newFamilyName");
    rules_ = new QComboBox;
    rules_->setObjectName("newFamilyRules");
    rules_->addItem("Within family", int(Promotion::FamilyRules::WithinFamily));
    rules_->addItem("Within family + Lich", int(Promotion::FamilyRules::WithLich));
    rules_->addItem("Within family + Lich / Vampire",
                    int(Promotion::FamilyRules::WithLichAndVampire));
    rules_->addItem("No manual class changes", int(Promotion::FamilyRules::NoManualChanges));
    rules_->setToolTip("Within family uses normal stat, equipment and compatibility checks. "
                       "It also permits category 0x08 changes between classes using this rule. "
                       "Lich/Vampire options add the game's special targets; their other "
                       "requirements still apply.");
    familyForm->addRow("Name", name_);
    familyForm->addRow("Class-change rules", rules_);
    auto add = new QPushButton("Create and assign family");
    add->setObjectName("createFamily");
    layout->addWidget(add);
    connect(add, &QPushButton::clicked, this, [this] {
        edit("Create class family", [this](Promotion &p) {
            p.assign(id_, p.createFamily(name_->text(),
                                         Promotion::FamilyRules(rules_->currentData().toInt())));
        });
    });
    auto detail =
        new QLabel("Creates a new family containing only this class. Assign other classes to it "
                   "using Class family above. No members or promotion requirements are copied. "
                   "Class-change rules control manual changes and optional special targets; "
                   "compatibility, stats, equipment and story conditions still apply.");
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
    const ViewPosition keepView(this);
    refreshing_ = true;
    const bool available = project_->promotion().has_value();
    const bool editable = available && id_ >= 1 && id_ < project_->promotion()->classCount();
    families_->setEnabled(editable);
    mask_->setEnabled(editable);
    equipmentSets_->setEnabled(editable);
    name_->setEnabled(editable);
    rules_->setEnabled(editable);
    findChild<QPushButton *>("createFamily")->setEnabled(editable);
    for (auto spin : gates_)
        spin->setEnabled(editable);
    notice_->setText(!available  ? "Promotion source owners are available for US Rev 0."
                     : !editable ? "This class is outside the existing manual-promotion roster."
                                 : "Manual class-change requirements. Equipment, story and "
                                   "character conditions also apply.");
    families_->clear();
    links_->clear();
    if (available) {
        const auto &p = *project_->promotion();
        for (auto f : p.families["families"].toArray()) {
            auto o = f.toObject();
            auto id = o["id"].toInt();
            auto label = QString("%1 · %2").arg(id).arg(o["name"].toString());
            families_->addItem(label, id);
        }
        if (editable) {
            families_->setCurrentIndex(
                families_->findData(p.families["assignments"].toArray()[id_].toInt()));
            for (int n = 0; n < gates_.size(); ++n)
                gates_[n]->setValue(u8(p.gates, id_ * 8 + n));
            const int mask=u8(p.masks,id_);
            mask_->clear();
            for (int value : {0,1,2,3,4,8,16,32,64,128}) mask_->addItem(compatibilityName(value),value);
            if(mask_->findData(mask)<0)mask_->addItem(compatibilityName(mask),mask);
            mask_->addItem("Custom combination…",-1);
            mask_->setCurrentIndex(mask_->findData(mask));
            equipmentSets_->setChecked((u8(p.flags, id_) & 0x80) != 0);
        }
    }
    const int family = families_->currentData().toInt();
    try {
        project_->loadCategoryIcons();
    } catch (const std::exception &) {
    }
    familyIcon_->clear();
    familyIcon_->addItem("Use existing categories", 0);
    if (project_->categoryIcons())
        for (int icon : project_->categoryIcons()->iconIds())
            familyIcon_->addItem(QPixmap::fromImage(project_->categoryIcons()->image(icon).scaled(
                                     28, 28, Qt::IgnoreAspectRatio, Qt::FastTransformation)),
                                 project_->categoryIcons()->label(icon), icon);
    const int selectedIcon =
        project_->categoryIcons() ? project_->categoryIcons()->familyIcon(family) : 0;
    familyIcon_->setCurrentIndex(familyIcon_->findData(selectedIcon));
    familyIconPreview_->setPixmap(
        selectedIcon
            ? QPixmap::fromImage(project_->categoryIcons()
                                     ->image(selectedIcon)
                                     .scaled(42, 42, Qt::IgnoreAspectRatio, Qt::FastTransformation))
            : QPixmap{});
    familyIcon_->setEnabled(editable && family > 0 && project_->categoryIcons().has_value());
    findChild<QPushButton *>("createFamilyIcon")->setEnabled(familyIcon_->isEnabled());
    findChild<QPushButton *>("editFamilyIcon")->setEnabled(project_->categoryIcons().has_value());
    linksTitle_->setText(QString("Link Family %1 to other families — shares manual class-change destinations. The current family is omitted.").arg(family));
    links_->setEnabled(editable && family != 0);
    findChild<QPushButton *>("applyFamilyLinks")->setEnabled(editable && family != 0);
    if (available && editable) {
        const auto &p = *project_->promotion();
        const auto selectedLinks = p.linkedFamilies(family);
        const auto assignments = p.families["assignments"].toArray();
        for (const auto &value : p.families["families"].toArray()) {
            const auto f = value.toObject();
            const int id = f["id"].toInt();
            if (!id || id == family)
                continue;
            QStringList examples;
            for (int cl = 1; cl < assignments.size() && examples.size() < 3; ++cl)
                if (assignments[cl] == id)
                    examples
                        << QString(project_->record(Table::Classes, cl).name).replace('\n', ' ');
            auto row =
                new QListWidgetItem(QString("%1 · %2%3")
                                        .arg(id)
                                        .arg(f["name"].toString(),
                                             examples.isEmpty() ? "" : " — " + examples.join(", ")),
                                    links_);
            row->setData(Qt::UserRole, id);
            row->setCheckState(selectedLinks.contains(id) ? Qt::Checked : Qt::Unchecked);
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
