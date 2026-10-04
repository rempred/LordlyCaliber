#include "app/encounter_editor.h"
#include <QComboBox>
#include <QCompleter>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
namespace lordly {
class EncounterCommand final : public QUndoCommand {
  public:
    EncounterCommand(Project *project, QString label, QJsonObject before, QJsonObject after)
        : QUndoCommand(label), project_(project), before_(std::move(before)),
          after_(std::move(after)) {}
    void undo() override {
        project_->setEncounters(before_);
    }
    void redo() override {
        project_->setEncounters(after_);
    }

  private:
    Project *project_;
    QJsonObject before_, after_;
};
EncounterEditor::EncounterEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                                 QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("encounterEditor");
    auto layout = new QVBoxLayout(this);
    auto tabs = new QTabWidget;
    layout->addWidget(tabs);
    auto poolPage = new QWidget;
    auto poolLayout = new QVBoxLayout(poolPage);
    search_ = new QLineEdit;
    search_->setObjectName("encounterSearch");
    search_->setPlaceholderText("Find a pool by mission, scenario key, or class…");
    search_->setClearButtonEnabled(true);
    poolLayout->addWidget(search_);
    auto split = new QSplitter;
    poolLayout->addWidget(split, 1);
    pools_ = new QListWidget;
    pools_->setObjectName("encounterPools");
    pools_->setMinimumWidth(220);
    split->addWidget(pools_);
    auto details = new QWidget;
    auto detail = new QVBoxLayout(details);
    heading_ = new QLabel;
    heading_->setWordWrap(true);
    detail->addWidget(heading_);
    auto explanation = new QLabel("Each terrain pair is drawn 50/50 after the encounter rate "
                                  "gates pass. Repeat a class for one outcome, or choose None "
                                  "for a result that produces no encounter.");
    explanation->setWordWrap(true);
    detail->addWidget(explanation);
    auto table = new QTableWidget(Encounters::slotCount, 3);
    table->setHorizontalHeaderLabels({"Terrain slot", "First outcome", "Second outcome"});
    table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    const auto terrains = Encounters::terrainNames();
    for (int slot = 0; slot < Encounters::slotCount; ++slot) {
        table->setItem(slot, 0, new QTableWidgetItem(terrains[slot]));
        table->setRowHeight(slot, 34);
        for (int side = 0; side < 2; ++side) {
            auto choice = new QComboBox;
            choice->setObjectName(QString("encounterClass_%1_%2").arg(slot).arg(side));
            choice->setEditable(true);
            choice->setInsertPolicy(QComboBox::NoInsert);
            choice->completer()->setCompletionMode(QCompleter::PopupCompletion);
            choice->completer()->setFilterMode(Qt::MatchContains);
            choice->completer()->setCaseSensitivity(Qt::CaseInsensitive);
            table->setCellWidget(slot, side + 1, choice);
            pairs_ << choice;
            connect(choice, &QComboBox::currentIndexChanged, this,
                    [this, slot] { changePair(slot); });
        }
    }
    detail->addWidget(table, 1);
    auto actions = new QHBoxLayout;
    auto copy = new QPushButton("Copy pool from…");
    copy->setObjectName("encounterCopy");
    copyFrom_ = new QComboBox;
    copyFrom_->setObjectName("encounterCopySource");
    auto restore = new QPushButton("Restore loaded pool");
    restore->setObjectName("encounterRestore");
    actions->addWidget(copy);
    actions->addWidget(copyFrom_, 1);
    actions->addWidget(restore);
    detail->addLayout(actions);
    notice_ = new QLabel;
    notice_->setWordWrap(true);
    detail->addWidget(notice_);
    split->addWidget(details);
    split->setSizes({300, 900});
    tabs->addTab(poolPage, "Encounter pools");
    auto ratePage = new QWidget;
    auto rateLayout = new QVBoxLayout(ratePage);
    auto rateExplanation =
        new QLabel("Terrain chance is the second encounter gate. The game's "
                   "global rate applies before it. A disabled pool slot prevents "
                   "encounters for that terrain ID.");
    rateExplanation->setWordWrap(true);
    rateLayout->addWidget(rateExplanation);
    auto rateTable = new QTableWidget(Encounters::terrainCount, 3);
    rateTable->setHorizontalHeaderLabels({"Terrain ID", "Chance (%)", "Encounter pool slot"});
    rateTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    rateTable->verticalHeader()->hide();
    rateTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for (int terrain = 0; terrain < Encounters::terrainCount; ++terrain) {
        rateTable->setItem(terrain, 0,
                           new QTableWidgetItem(
                               QString("%1 (0x%2)").arg(terrain).arg(terrain, 2, 16, QChar('0'))));
        auto rate = new QSpinBox;
        rate->setObjectName(QString("encounterRate_%1").arg(terrain));
        rate->setRange(0, 100);
        auto lookup = new QComboBox;
        lookup->setObjectName(QString("encounterSlot_%1").arg(terrain));
        lookup->addItem("Disabled", 0);
        for (int slot = 0; slot < terrains.size(); ++slot)
            lookup->addItem(terrains[slot], slot + 1);
        rateTable->setCellWidget(terrain, 1, rate);
        rateTable->setCellWidget(terrain, 2, lookup);
        rateTable->setRowHeight(terrain, 32);
        rates_ << rate;
        lookups_ << lookup;
        connect(rate, &QSpinBox::valueChanged, this, [this, terrain] { changeTerrain(terrain); });
        connect(lookup, &QComboBox::currentIndexChanged, this,
                [this, terrain] { changeTerrain(terrain); });
    }
    rateLayout->addWidget(rateTable);
    tabs->addTab(ratePage, "Terrain rates");
    for (int id = 1; id <= Encounters::poolCount; ++id) {
        auto item = new QListWidgetItem(project_->encounters()->label(id), pools_);
        item->setData(Qt::UserRole, id);
        copyFrom_->addItem(item->text(), id);
    }
    connect(search_, &QLineEdit::textChanged, this, [this] { filter(); });
    connect(pools_, &QListWidget::currentRowChanged, this, [this] { refresh(); });
    connect(copy, &QPushButton::clicked, this, [this] {
        if (selected() < 1)
            return;
        auto next = *project_->encounters();
        const int source = copyFrom_->currentData().toInt();
        for (int slot = 0; slot < Encounters::slotCount; ++slot)
            next.setPair(selected(), slot, next.classAt(source, slot, 0),
                         next.classAt(source, slot, 1),
                         int(project_->records(Table::Classes).size()));
        commit(next, "Copy encounter pool");
    });
    connect(restore, &QPushButton::clicked, this, [this] {
        if (selected() < 1)
            return;
        auto next = *project_->encounters();
        const auto original =
            Encounters::load(project_->rom(), int(project_->records(Table::Classes).size()));
        for (int slot = 0; slot < Encounters::slotCount; ++slot)
            next.setPair(selected(), slot, original.classAt(selected(), slot, 0),
                         original.classAt(selected(), slot, 1),
                         int(project_->records(Table::Classes).size()));
        commit(next, "Restore encounter pool");
    });
    connect(undo_, &QUndoStack::indexChanged, this, [this] {
        refresh();
        filter();
        changed_();
    });
    pools_->setCurrentRow(0);
    refresh();
}
int EncounterEditor::selected() const {
    return pools_->currentItem() ? pools_->currentItem()->data(Qt::UserRole).toInt() : -1;
}
void EncounterEditor::refresh() {
    const auto &encounters = *project_->encounters();
    const int pool = selected();
    if (pool >= 1) {
        QStringList keys;
        for (const int key : encounters.scenarioKeys(pool))
            keys << QString::number(key);
        heading_->setText(encounters.label(pool) +
                          (keys.isEmpty() ? "\nNo scenario references in the loaded metadata."
                                          : "\nShared by scenario keys: " + keys.join(", ")));
        for (int slot = 0; slot < Encounters::slotCount; ++slot)
            for (int side = 0; side < 2; ++side) {
                auto combo = pairs_[slot * 2 + side];
                QSignalBlocker block(combo);
                combo->clear();
                combo->addItem("0 · None", 0);
                for (int id = 1; id < project_->records(Table::Classes).size(); ++id)
                    combo->addItem(
                        QString("%1 · %2").arg(id).arg(
                            QString(project_->record(Table::Classes, id).name).replace('\n', ' ')),
                        id);
                combo->setCurrentIndex(combo->findData(encounters.classAt(pool, slot, side)));
            }
    }
    for (int t = 0; t < Encounters::terrainCount; ++t) {
        QSignalBlocker rateBlock(rates_[t]), lookupBlock(lookups_[t]);
        rates_[t]->setValue(encounters.rate(t));
        rates_[t]->setEnabled(encounters.lookup(t) != 0);
        lookups_[t]->setCurrentIndex(lookups_[t]->findData(encounters.lookup(t)));
    }
    notice_->clear();
}
void EncounterEditor::filter() {
    const auto query = search_->text();
    for (int row = 0; row < pools_->count(); ++row) {
        auto item = pools_->item(row);
        const int pool = item->data(Qt::UserRole).toInt();
        QString searchable = item->text();
        for (const int key : project_->encounters()->scenarioKeys(pool))
            searchable += " " + QString::number(key);
        for (int slot = 0; slot < Encounters::slotCount; ++slot)
            for (int side = 0; side < 2; ++side)
                searchable += " " + project_
                                        ->record(Table::Classes,
                                                 project_->encounters()->classAt(pool, slot, side))
                                        .name;
        item->setHidden(!searchable.contains(query, Qt::CaseInsensitive));
    }
}
void EncounterEditor::commit(const Encounters &next, const QString &label) {
    const auto before = project_->encounters()->json(), after = next.json();
    if (before != after)
        undo_->push(new EncounterCommand(project_, label, before, after));
}
void EncounterEditor::changePair(int slot) {
    if (selected() < 1 || pairs_[slot * 2]->currentIndex() < 0 ||
        pairs_[slot * 2 + 1]->currentIndex() < 0)
        return;
    try {
        auto next = *project_->encounters();
        next.setPair(selected(), slot, pairs_[slot * 2]->currentData().toInt(),
                     pairs_[slot * 2 + 1]->currentData().toInt(),
                     int(project_->records(Table::Classes).size()));
        commit(next, "Edit encounter pair");
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
void EncounterEditor::changeTerrain(int terrain) {
    try {
        auto next = *project_->encounters();
        const int slot = lookups_[terrain]->currentData().toInt();
        next.setTerrain(terrain, slot ? rates_[terrain]->value() : 0, slot);
        commit(next, "Edit encounter terrain rate");
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
} // namespace lordly
