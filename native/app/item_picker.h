#pragma once
#include "engine/project.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
namespace lordly {
inline QVector<quint16> chooseRomItems(Project *project, QWidget *parent, bool equipment,
                                      const QVector<quint16> &existing, int remaining,
                                      bool reward = false) {
    QDialog dialog(parent);
    dialog.setObjectName("shopItemPicker");
    dialog.setWindowTitle(reward ? "Choose reward" : equipment ? "Add equipment to shop" : "Add consumables to shop");
    dialog.resize(780, 720);
    auto layout = new QVBoxLayout(&dialog);
    int switchKind = -1;
    if (reward) {
        auto kinds = new QHBoxLayout;
        for (int kind = 0; kind < 3; ++kind) {
            auto button = new QPushButton(kind == 0 ? "Equipment" : kind == 1 ? "Consumables" : "No item");
            button->setObjectName(QString("rewardPickerKind_%1").arg(kind));
            button->setEnabled(kind == 2 || (kind == 0) != equipment);
            kinds->addWidget(button);
            QObject::connect(button, &QPushButton::clicked, &dialog, [&, kind] { switchKind = kind; dialog.reject(); });
        }
        layout->addLayout(kinds);
    }
    auto search = new QLineEdit;
    search->setObjectName("shopItemSearch");
    search->setPlaceholderText("Find an item by name or ID…");
    layout->addWidget(search);
    auto types = new QButtonGroup(&dialog);
    types->setExclusive(true);
    auto filters = new QGridLayout;
    const auto records = project->records(equipment ? Table::Items : Table::Consumables);
    QMap<int, int> representatives;
    for (const auto &record : records)
        if (record.id &&
            !representatives.contains(equipment ? u8(record.bytes, 4) : u16(record.bytes, 4)))
            representatives[equipment ? u8(record.bytes, 4) : u16(record.bytes, 4)] = record.id;
    const auto names = project->catalog()["EQUIP_TYPES"].toObject();
    int buttonIndex = 0;
    auto typeButton = [&](int type, const QString &label, int example) {
        auto button = new QPushButton(label);
        button->setCheckable(true);
        button->setObjectName(QString("shopItemType_%1").arg(type));
        if (example > 0)
            button->setIcon(QPixmap::fromImage(
                project->iconArtwork()->image(example | (equipment ? 0 : 0x200))));
        button->setIconSize(QSize(24, 24));
        types->addButton(button, type);
        filters->addWidget(button, buttonIndex / 4, buttonIndex % 4);
        ++buttonIndex;
        return button;
    };
    typeButton(-2, "All types", 0)->setChecked(true);
    const QMap<int, QString> consumableTypes{{0, "Recovery"}, {256, "Character boosts"}, {512, "Escape"}, {513, "Special use"}, {768, "Carried effects"}, {1025, "Story items"}};
    for (auto it = representatives.cbegin(); it != representatives.cend(); ++it)
        typeButton(it.key(),
                   equipment
                       ? names[QString::number(it.key())].toString(QString("Type %1").arg(it.key()))
                       : consumableTypes.value(it.key(), QString("Category %1").arg(it.key())),
                   it.value());
    layout->addLayout(filters);
    auto list = new QListWidget;
    list->setObjectName("shopItemChoices");
    list->setIconSize(QSize(48, 48));
    list->setSelectionMode(remaining == 1 ? QAbstractItemView::SingleSelection : QAbstractItemView::ExtendedSelection);
    layout->addWidget(list, 1);
    for (const auto &record : project->records(equipment ? Table::Items : Table::Consumables)) {
        if (!record.id || existing.contains(record.id))
            continue;
        auto row = new QListWidgetItem(
            QString("%1 · %2").arg(record.id).arg(record.name), list);
        row->setData(Qt::UserRole, record.id);
        row->setData(Qt::UserRole + 1, equipment ? u8(record.bytes, 4) : u16(record.bytes, 4));
        row->setIcon(
            QPixmap::fromImage(project->iconArtwork()->image(record.id | (equipment ? 0 : 0x200)))
                .scaled(48, 48, Qt::KeepAspectRatio, Qt::FastTransformation));
    }
    auto help = new QLabel(
        QString("Select up to %1 items. Icons are decoded from the current ROM and project edits.")
            .arg(remaining));
    help->setWordWrap(true);
    layout->addWidget(help);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(reward ? "Choose selected" : "Add selected");
    buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(list, &QListWidget::itemSelectionChanged, &dialog, [=] {
        const int count = list->selectedItems().size();
        buttons->button(QDialogButtonBox::Ok)->setEnabled(count > 0 && count <= remaining);
    });
    const auto filterItems = [=] {
        const int type = types->checkedId();
        for (int i = 0; i < list->count(); ++i) {
            auto row = list->item(i);
            row->setHidden((type != -2 && row->data(Qt::UserRole + 1).toInt() != type) ||
                           !row->text().contains(search->text(), Qt::CaseInsensitive));
            if (row->isHidden())
                row->setSelected(false);
        }
    };
    QObject::connect(search, &QLineEdit::textChanged, &dialog, filterItems);
    QObject::connect(types, &QButtonGroup::idClicked, &dialog, filterItems);
    QVector<quint16> result;
    if (dialog.exec() == QDialog::Accepted)
        for (auto row : list->selectedItems())
            result << quint16(row->data(Qt::UserRole).toInt());
    if (switchKind == 2) return {0};
    if (switchKind >= 0) return chooseRomItems(project, parent, switchKind == 0, {}, 1, true);
    if (reward && equipment)
        for (auto &id : result) id |= 0x8000;
    return result;
}
class RewardItemPicker : public QComboBox {
  public:
    explicit RewardItemPicker(Project *project) : project_(project) { setEditable(false); }
    void showPopup() override {
        project_->loadIconArtwork();
        const auto choice = chooseRomItems(project_, this, !currentData().toInt() || (currentData().toInt() & 0x8000), {}, 1, true);
        if (!choice.isEmpty()) setCurrentIndex(findData(choice[0]));
    }
  private:
    Project *project_;
};
} // namespace lordly
