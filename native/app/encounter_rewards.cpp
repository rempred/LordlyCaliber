#include "app/encounter_editor.h"
#include <QComboBox>
#include <QSet>
#include <QCompleter>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
namespace lordly {
QWidget *EncounterEditor::createRewardsPage() {
    auto page = new QWidget;
    page->setObjectName("creatureRewardsPage");
    auto layout = new QVBoxLayout(page);
    auto explanation = new QLabel(
        "Rewards for defeated neutral units are selected by class. "
        "Each class has three outcomes with relative weights. A zero weight excludes an "
        "outcome; None with a positive weight gives a chance of no reward. Mission squad "
        "rewards and scripted gifts are separate.");
    explanation->setWordWrap(true);
    layout->addWidget(explanation);
    rewardSearch_ = new QLineEdit;
    rewardSearch_->setObjectName("rewardSearch");
    rewardSearch_->setPlaceholderText("Find a class by name or ID…");
    rewardSearch_->setClearButtonEnabled(true);
    layout->addWidget(rewardSearch_);
    auto split = new QSplitter;
    layout->addWidget(split, 1);
    rewardClasses_ = new QListWidget;
    rewardClasses_->setObjectName("rewardClasses");
    split->addWidget(rewardClasses_);
    auto details = new QWidget;
    rewardDetails_ = details;
    auto detail = new QVBoxLayout(details);
    auto table = new QTableWidget(3, 3);
    table->setHorizontalHeaderLabels({"Outcome", "Weight", "Relative share"});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setMaximumHeight(160);
    detail->addWidget(table);
    for (int slot = 0; slot < 3; ++slot) {
        table->setRowHeight(slot, 36);
        auto item = new QComboBox;
        item->setObjectName(QString("rewardItem_%1").arg(slot));
        item->setEditable(true);
        item->setInsertPolicy(QComboBox::NoInsert);
        item->completer()->setCompletionMode(QCompleter::PopupCompletion);
        item->completer()->setFilterMode(Qt::MatchContains);
        item->completer()->setCaseSensitivity(Qt::CaseInsensitive);
        item->addItem("None", 0);
        for (const auto kind : {Table::Items, Table::Consumables})
            for (int id = 1; id < project_->records(kind).size(); ++id)
                item->addItem(QString("%1 %2 · %3")
                                  .arg(kind == Table::Items ? "Equipment" : "Consumable")
                                  .arg(id)
                                  .arg(QString(project_->record(kind, id).name).replace('\n', ' ')),
                              (kind == Table::Items ? 0x8000 : 0) | id);
        table->setCellWidget(slot, 0, item);
        rewardItems_ << item;
        auto weight = new QSpinBox;
        weight->setObjectName(QString("rewardWeight_%1").arg(slot));
        weight->setRange(0, 65535);
        table->setCellWidget(slot, 1, weight);
        rewardWeights_ << weight;
        auto odds = new QLabel;
        odds->setMargin(8);
        table->setCellWidget(slot, 2, odds);
        rewardOdds_ << odds;
        connect(weight, &QSpinBox::valueChanged, this, [this] { showRewardOdds(); });
    }
    auto actions = new QHBoxLayout;
    auto apply = new QPushButton("Apply rewards");
    apply->setObjectName("rewardApply");
    auto restore = new QPushButton("Restore loaded rewards");
    restore->setObjectName("rewardRestore");
    auto clear = new QPushButton("No rewards");
    clear->setObjectName("rewardClear");
    actions->addWidget(apply);
    actions->addWidget(restore);
    actions->addWidget(clear);
    detail->addLayout(actions);
    auto copyLayout = new QHBoxLayout;
    auto copy = new QPushButton("Copy rewards from…");
    copy->setObjectName("rewardCopy");
    rewardCopyFrom_ = new QComboBox;
    rewardCopyFrom_->setObjectName("rewardCopySource");
    copyLayout->addWidget(copy);
    copyLayout->addWidget(rewardCopyFrom_, 1);
    detail->addLayout(copyLayout);
    rewardNotice_ = new QLabel;
    rewardNotice_->setObjectName("rewardNotice");
    rewardNotice_->setWordWrap(true);
    detail->addWidget(rewardNotice_);
    detail->addStretch();
    split->addWidget(details);
    split->setSizes({300, 900});
    connect(rewardSearch_, &QLineEdit::textChanged, this, [this] { filterRewards(); });
    connect(rewardClasses_, &QListWidget::currentRowChanged, this, [this] { showReward(); });
    connect(apply, &QPushButton::clicked, this, [this] { applyReward(false); });
    connect(restore, &QPushButton::clicked, this, [this] { applyReward(true); });
    connect(clear, &QPushButton::clicked, this, [this] {
        if (!rewardClasses_->currentItem())
            return;
        auto next = *project_->encounters();
        next.setReward(rewardClasses_->currentItem()->data(Qt::UserRole).toInt(), CreatureReward{});
        commit(next, "Remove class rewards");
        showReward();
    });
    connect(copy, &QPushButton::clicked, this, [this] {
        if (!rewardClasses_->currentItem())
            return;
        auto next = *project_->encounters();
        next.setReward(rewardClasses_->currentItem()->data(Qt::UserRole).toInt(),
                       next.rewards().row(rewardCopyFrom_->currentData().toInt()));
        commit(next, "Copy class rewards");
        showReward();
    });
    return page;
}
void EncounterEditor::refreshRewardClasses() {
    if (!rewardClasses_)
        return;
    for (auto box : rewardItems_) {
        const QSignalBlocker blocker(box);
        box->clear();
        box->addItem("None", 0);
        for (const auto kind : {Table::Items, Table::Consumables})
            for (int id = 1; id < project_->records(kind).size(); ++id)
                box->addItem(QString("%1 %2 · %3")
                                 .arg(kind == Table::Items ? "Equipment" : "Consumable")
                                 .arg(id)
                                 .arg(project_->record(kind, id).name),
                             (kind == Table::Items ? 0x8000 : 0) | id);
    }
    const auto selected = rewardClasses_->currentItem()
                              ? rewardClasses_->currentItem()->data(Qt::UserRole).toInt()
                              : -1;
    const auto copy = rewardCopyFrom_->currentData().toInt();
    QSignalBlocker block(rewardClasses_), copyBlock(rewardCopyFrom_);
    rewardClasses_->clear();
    rewardCopyFrom_->clear();
    QSet<int> used;
    for (int pool = 1; pool <= Encounters::poolCount; ++pool)
        for (int slot = 0; slot < Encounters::slotCount; ++slot)
            for (int side = 0; side < 2; ++side)
                used.insert(project_->encounters()->classAt(pool, slot, side));
    for (const auto &squad : project_->encounters()->customSquads().profiles())
        for (const auto &member : squad.members)
            used.insert(member.classId);
    QVector<int> ordered;
    for (bool active : {true, false})
        for (int id = 1; id < project_->records(Table::Classes).size(); ++id)
            if (used.contains(id) == active)
                ordered << id;
    int selectedRow = 0;
    for (const int id : ordered) {
        if (id == selected)
            selectedRow = rewardClasses_->count();
        const auto label = QString("%1 · %2").arg(id).arg(
            QString(project_->record(Table::Classes, id).name).replace('\n', ' '));
        auto item = new QListWidgetItem(label, rewardClasses_);
        item->setData(Qt::UserRole, id);
        item->setToolTip(used.contains(id) ? "Used in an encounter pool or custom squad" : "Not currently used in encounters");
        rewardCopyFrom_->addItem(label, id);
    }
    rewardClasses_->setCurrentRow(selectedRow);
    rewardCopyFrom_->setCurrentIndex(qMax(0, rewardCopyFrom_->findData(copy)));
    filterRewards();
    showReward();
}
void EncounterEditor::filterRewards() {
    const auto query = rewardSearch_->text().trimmed();
    for (int i = 0; i < rewardClasses_->count(); ++i) {
        auto item = rewardClasses_->item(i);
        item->setHidden(!query.isEmpty() && !item->text().contains(query, Qt::CaseInsensitive));
    }
    if (!rewardClasses_->currentItem() || rewardClasses_->currentItem()->isHidden()) {
        int first = -1;
        for (int i = 0; i < rewardClasses_->count(); ++i)
            if (!rewardClasses_->item(i)->isHidden()) {
                first = i;
                break;
            }
        rewardClasses_->setCurrentRow(first);
    }
}
void EncounterEditor::showReward() {
    rewardDetails_->setEnabled(rewardClasses_->currentItem() != nullptr);
    if (!rewardClasses_->currentItem()) {
        rewardNotice_->setText("No matching class.");
        return;
    }
    const auto &row = project_->encounters()->rewards().row(
        rewardClasses_->currentItem()->data(Qt::UserRole).toInt());
    for (int i = 0; i < 3; ++i) {
        QSignalBlocker block(rewardWeights_[i]);
        rewardItems_[i]->setCurrentIndex(rewardItems_[i]->findData(row.outcomes[i]));
        rewardWeights_[i]->setValue(row.weights[i]);
    }
    rewardNotice_->clear();
    showRewardOdds();
}
void EncounterEditor::showRewardOdds() {
    if (rewardWeights_.size() != 3)
        return;
    int total = 0;
    for (auto w : rewardWeights_)
        total += w->value();
    for (int i = 0; i < 3; ++i)
        rewardOdds_[i]->setText(
            total ? QString("%1%").arg(100.0 * rewardWeights_[i]->value() / total, 0, 'f', 4)
                  : "Invalid total");
}
void EncounterEditor::applyReward(bool restore) {
    if (!rewardClasses_->currentItem())
        return;
    try {
        const int id = rewardClasses_->currentItem()->data(Qt::UserRole).toInt();
        CreatureReward value;
        if (restore)
            value = CreatureRewards::load(project_->rom()).row(id);
        else
            for (int i = 0; i < 3; ++i) {
                if (rewardItems_[i]->currentIndex() < 0 ||
                    rewardItems_[i]->currentText() !=
                        rewardItems_[i]->itemText(rewardItems_[i]->currentIndex()))
                    throw Error("Choose a reward from the item list");
                value.outcomes[i] = rewardItems_[i]->currentData().toInt();
                value.weights[i] = rewardWeights_[i]->value();
            }
        auto next = *project_->encounters();
        next.setReward(id, value);
        commit(next, restore ? "Restore class rewards" : "Edit class rewards");
        showReward();
    } catch (const std::exception &e) {
        rewardNotice_->setText(QString::fromUtf8(e.what()));
    }
}
} // namespace lordly
