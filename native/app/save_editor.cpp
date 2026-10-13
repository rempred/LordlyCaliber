#include "app/view_position.h"
#include "app/save_editor.h"
#include "app/save_unit_editor.h"
#include <QAbstractTableModel>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>
namespace lordly {
namespace {
enum { MinimumRole = Qt::UserRole, MaximumRole, ChoicesRole, NameLengthRole };
class SaveCommand final : public QUndoCommand {
  public:
    SaveCommand(SaveEditor *editor, SaveChange change, const QString &label)
        : QUndoCommand("Edit save " + label), editor_(editor), change_(std::move(change)) {}
    void undo() override {
        editor_->apply(change_, false);
    }
    void redo() override {
        editor_->apply(change_, true);
    }

  private:
    SaveEditor *editor_;
    SaveChange change_;
};
class SaveModel final : public QAbstractTableModel {
  public:
    SaveModel(SaveEditor *editor, SaveTable table)
        : QAbstractTableModel(editor), editor_(editor), table_(table) {
        connect(editor, &SaveEditor::refreshed, this, [this] {
            beginResetModel();
            endResetModel();
        });
    }
    int rowCount(const QModelIndex &parent = {}) const override {
        return parent.isValid() || !editor_->document()
                   ? 0
                   : int(editor_->document()->rows(table_).size());
    }
    int columnCount(const QModelIndex &parent = {}) const override {
        return parent.isValid() || !editor_->document()
                   ? 0
                   : int(editor_->document()->fields(table_).size()) + 1;
    }
    QVariantList choices(const SaveField &field) const {
        QVariantList result;
        if (field.key == "element") {
            for (const auto &label : {"None", "Wind", "Flame", "Earth", "Water"})
                result.append(QString::fromLatin1(label));
            return result;
        }
        std::optional<Table> catalog;
        if (field.key == "class" || field.key == "secondaryClass")
            catalog = Table::Classes;
        else if (field.key == "weapon" || field.key == "body" || field.key == "offhand" ||
                 field.key == "head" || (field.key == "id" && table_ == SaveTable::Equipment))
            catalog = Table::Items;
        else if (field.key == "id" && table_ == SaveTable::Consumables)
            catalog = Table::Consumables;
        if (catalog)
            for (const auto &record : editor_->project()->records(*catalog))
                result.append(QString("%1 — %2").arg(record.id).arg(
                    record.id ? record.name.simplified() : QString("Default / none")));
        return result;
    }
    QVariant data(const QModelIndex &index, int role) const override {
        if (!index.isValid() || !editor_->document())
            return {};
        const auto &doc = *editor_->document();
        const auto rows = doc.rows(table_);
        if (index.row() >= rows.size() || index.column() >= columnCount())
            return {};
        if (!index.column())
            return role == Qt::DisplayRole ? QVariant(rows[index.row()]) : QVariant{};
        const auto field = doc.fields(table_)[index.column() - 1];
        const auto value = doc.value(table_, rows[index.row()], field.key);
        if (role == Qt::EditRole)
            return value;
        if (role == MinimumRole)
            return field.minimum;
        if (role == MaximumRole)
            return field.maximum;
        if (role == ChoicesRole)
            return choices(field);
        if (role == NameLengthRole)
            return field.key == "name" ? field.bytes - 1 : 0;
        if (role == Qt::DisplayRole) {
            const auto labels = choices(field);
            if (!labels.isEmpty())
                return value.toUInt() < quint32(labels.size())
                           ? labels[value.toInt()]
                           : QVariant(QString("Unknown (%1)").arg(value.toUInt()));
            return value;
        }
        return {};
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal || !editor_->document())
            return {};
        return !section ? "Record" : editor_->document()->fields(table_)[section - 1].label;
    }
    Qt::ItemFlags flags(const QModelIndex &index) const override {
        return QAbstractTableModel::flags(index) |
               (index.column() ? Qt::ItemIsEditable : Qt::NoItemFlags);
    }
    bool setData(const QModelIndex &index, const QVariant &value, int role) override {
        if (role != Qt::EditRole || !index.isValid() || !index.column() || !editor_->document())
            return false;
        const auto &doc = *editor_->document();
        return editor_->edit(table_, doc.rows(table_)[index.row()],
                             doc.fields(table_)[index.column() - 1].key, value);
    }

  private:
    SaveEditor *editor_;
    SaveTable table_;
};
class SaveDelegate final : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &index) const override {
        const auto choices = index.data(ChoicesRole).toList();
        if (!choices.isEmpty()) {
            auto box = new QComboBox(parent);
            for (int i = int(index.data(MinimumRole).toUInt()); i < choices.size(); ++i)
                box->addItem(choices[i].toString(), i);
            const auto value = index.data(Qt::EditRole).toUInt();
            if (box->findData(value) < 0)
                box->addItem(QString("Keep unknown (%1)").arg(value), value);
            return box;
        }
        if (index.data(NameLengthRole).toInt()) {
            auto text = new QLineEdit(parent);
            text->setMaxLength(index.data(NameLengthRole).toInt());
            return text;
        }
        auto number = new QSpinBox(parent);
        number->setRange(index.data(MinimumRole).toInt(), index.data(MaximumRole).toInt());
        number->setKeyboardTracking(false);
        return number;
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override {
        const auto value = index.data(Qt::EditRole);
        if (auto box = qobject_cast<QComboBox *>(editor))
            box->setCurrentIndex(box->findData(value));
        else if (auto text = qobject_cast<QLineEdit *>(editor)) {
            text->setMaxLength(std::max(text->maxLength(), int(value.toString().size())));
            text->setText(value.toString());
        } else {
            auto number = qobject_cast<QSpinBox *>(editor);
            number->setRange(std::min(number->minimum(), value.toInt()),
                             std::max(number->maximum(), value.toInt()));
            number->setValue(value.toInt());
        }
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override {
        QVariant value;
        if (auto box = qobject_cast<QComboBox *>(editor))
            value = box->currentData();
        else if (auto text = qobject_cast<QLineEdit *>(editor))
            value = text->text();
        else
            value = qobject_cast<QSpinBox *>(editor)->value();
        model->setData(index, value, Qt::EditRole);
    }
};
} // namespace
SaveEditor::SaveEditor(Project *project, QWidget *parent)
    : QWidget(parent), project_(project), undo_(new QUndoStack(this)) {
    setObjectName("saveEditor");
    auto layout = new QVBoxLayout(this);
    auto actions = new QHBoxLayout;
    auto open = new QPushButton("Open save…");
    open->setObjectName("openSave");
    export_ = new QPushButton("Export edited save…");
    export_->setObjectName("exportSave");
    restore_ = new QPushButton("Restore loaded save");
    restore_->setObjectName("restoreSave");
    actions->addWidget(open);
    actions->addWidget(export_);
    actions->addWidget(restore_);
    defaultArmy_ = new QPushButton("Use as Tester Army");
    defaultArmy_->setObjectName("saveDefaultArmy");
    defaultArmy_->setToolTip("Save this army for Testing, then return to the mission list. Story "
                             "progress and loose inventory are supplied by the seeder.");
    defaultArmy_->hide();
    actions->addWidget(defaultArmy_);
    connect(defaultArmy_, &QPushButton::clicked, this, [this] {
        if (!defaultArmySink_ || !document_)
            return;
        try {
            if (pendingUnitEdits_)
                throw Error("Apply or discard pending squad changes first");
            auto prepared = *document_;
            if (prepared.selectedSlot() != 0)
                throw Error("Edit normal slot 1 for the default army");
            const auto changes = prepared.planArmyRefresh(*project_);
            if (!changes.spans.isEmpty())
                prepared.apply(changes, true);
            prepared.validateEquipmentEdits(int(project_->records(Table::Items).size()));
            prepared.validateClassEdits(int(project_->records(Table::Classes).size()));
            prepared.validateUnitSupplyEdits(*project_);
            defaultArmySink_(prepared.serialize());
            if (!changes.spans.isEmpty())
                undo_->push(new SaveCommand(this, changes, "refresh default army"));
            undo_->setClean();
        } catch (const std::exception &e) {
            error_->setText(QString::fromUtf8(e.what()));
        }
    });
    actions->addStretch();
    undoButton_ = new QPushButton("Undo");
    redoButton_ = new QPushButton("Redo");
    undoButton_->setObjectName("saveUndo");
    redoButton_->setObjectName("saveRedo");
    connect(undoButton_, &QPushButton::clicked, this, [this] {
        if (!pendingUnitEdits_)
            undo_->undo();
    });
    connect(redoButton_, &QPushButton::clicked, this, [this] {
        if (!pendingUnitEdits_)
            undo_->redo();
    });
    actions->addWidget(undoButton_);
    actions->addWidget(redoButton_);
    layout->addLayout(actions);
    status_ = new QLabel;
    status_->setWordWrap(true);
    layout->addWidget(status_);
    slots_ = new QComboBox;
    slots_->setObjectName("saveSlot");
    layout->addWidget(slots_);
    error_ = new QLabel;
    error_->setObjectName("saveError");
    error_->setWordWrap(true);
    layout->addWidget(error_);
    tables_ = new QTabWidget;
    layout->addWidget(tables_, 1);
    for (const auto &[table, title, id] :
         {std::tuple{SaveTable::Characters, "Characters", "saveCharacters"},
          std::tuple{SaveTable::Equipment, "Equipment", "saveEquipment"},
          std::tuple{SaveTable::Consumables, "Consumables / treasure", "saveConsumables"},
          std::tuple{SaveTable::Game, "Game state", "saveGameState"}}) {
        auto page = new QWidget;
        auto inner = new QVBoxLayout(page);
        auto search = new QLineEdit;
        search->setObjectName(QString(id) + "Search");
        search->setPlaceholderText("Search records, names and values…");
        search->setClearButtonEnabled(true);
        inner->addWidget(search);
        auto model = new SaveModel(this, table);
        auto filter = new QSortFilterProxyModel(page);
        filter->setSourceModel(model);
        filter->setFilterKeyColumn(-1);
        filter->setFilterCaseSensitivity(Qt::CaseInsensitive);
        auto grid = new QTableView;
        grid->setObjectName(id);
        grid->setModel(filter);
        grid->setItemDelegate(new SaveDelegate(grid));
        grid->setAlternatingRowColors(true);
        grid->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        grid->horizontalHeader()->setStretchLastSection(true);
        inner->addWidget(grid);
        if (table == SaveTable::Characters) {
            auto buttons = new QHBoxLayout;
            auto add = new QPushButton("Add character…");
            auto remove = new QPushButton("Remove selected reserve character");
            add->setObjectName("saveAddCharacter");
            remove->setObjectName("saveRemoveCharacter");
            buttons->addWidget(add);
            buttons->addWidget(remove);
            buttons->addStretch();
            inner->addLayout(buttons);
            const auto selectedId = [grid] {
                return grid->currentIndex().isValid()
                           ? grid->model()->index(grid->currentIndex().row(), 0).data().toInt()
                           : -1;
            };
            const auto update = [this, add, remove, selectedId] {
                add->setEnabled(document_ && document_->freeCharacterSlot() >= 0);
                remove->setEnabled(document_ && document_->reserveMembers().contains(selectedId()));
            };
            connect(this, &SaveEditor::refreshed, this, update);
            connect(grid->selectionModel(), &QItemSelectionModel::currentChanged, this, update);
            connect(remove, &QPushButton::clicked, this,
                    [this, selectedId] { removeCharacter(selectedId()); });
            connect(add, &QPushButton::clicked, this, [this] {
                const int id = promptAddCharacter();
                if (id >= 0) selectCharacter(id);
            });
            update();
        }
        connect(search, &QLineEdit::textChanged, filter,
                &QSortFilterProxyModel::setFilterFixedString);
        tables_->addTab(page, title);
    }
    auto army = new SaveUnitEditor(this);
    tables_->addTab(army, "Army Management");
    tables_->setCurrentWidget(army);
    auto note = new QLabel(
        "Save files have their own undo history and are exported separately from ROM projects. "
        "Character IDs, body codes, fatigue and unknown fields are preserved.");
    note->setWordWrap(true);
    layout->addWidget(note);
    connect(open, &QPushButton::clicked, this, [this] {
        if (!confirmDiscard())
            return;
        const auto path = QFileDialog::getOpenFileName(
            this, "Open save", {},
            "Save files (*.sra *.SaveRAM *.saveram *.state* *.bin);;All files (*)");
        if (path.isEmpty())
            return;
        try {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                throw Error(file.errorString());
            if (file.size() > 128 * 1024 * 1024)
                throw Error("Save file exceeds the supported size");
            loadBytes(file.readAll(), path);
        } catch (const Error &e) {
            error_->setText(QString::fromUtf8(e.what()));
        }
    });
    connect(export_, &QPushButton::clicked, this, [this] {
        if (!document_)
            return;
        const QFileInfo original(sourcePath_);
        const auto suggested = original.dir().filePath(original.completeBaseName() + "-edited" +
                                                       document_->defaultExtension());
        const auto path = QFileDialog::getSaveFileName(this, "Export edited save", suggested);
        if (!path.isEmpty())
            try {
                exportFile(path);
            } catch (const Error &e) {
                error_->setText(QString::fromUtf8(e.what()));
            }
    });
    connect(restore_, &QPushButton::clicked, this, [this] { undo_->setIndex(0); });
    connect(slots_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!document_ || index < 0)
            return;
        if (!allowImmediateEdit()) {
            const QSignalBlocker guard(slots_);
            slots_->setCurrentIndex(slots_->findData(document_->selectedSlot()));
            return;
        }
        try {
            document_->selectSlot(slots_->itemData(index).toInt());
            refresh();
        } catch (const Error &e) {
            error_->setText(QString::fromUtf8(e.what()));
        }
    });
    connect(undo_, &QUndoStack::indexChanged, this, [this] {
        refresh();
        emit changed();
    });
    connect(undo_, &QUndoStack::cleanChanged, this, [this] {
        refresh();
        emit changed();
    });
    refresh();
}
bool SaveEditor::confirmDiscard() {
    return !modified() ||
           QMessageBox::question(this, "Unsaved save edits", "Discard edits to the loaded save?",
                                 QMessageBox::Discard | QMessageBox::Cancel,
                                 QMessageBox::Cancel) == QMessageBox::Discard;
}
SaveEditor::~SaveEditor() {
    // QWidget deletes its child stack after our document member has gone away.
    // QUndoStack destruction emits clear/index signals; do not refresh then.
    disconnect(undo_, nullptr, this, nullptr);
    // Hiding/deleting a visible table asks its model for header geometry.
    // Detach the views while the document and project are still alive.
    for (auto grid : findChildren<QTableView *>())
        grid->setModel(nullptr);
}
void SaveEditor::loadBytes(const QByteArray &bytes, const QString &sourcePath) {
    auto candidate =
        SaveDocument::open(project_->runtimeRom(), bytes, int(project_->records(Table::Classes).size()));
    pendingUnitEdits_ = false; // The caller has already handled replacing the loaded document.
    defaultArmySink_ = {};
    defaultArmy_->hide();
    undo_->clear();
    document_ = std::move(candidate);
    sourcePath_ = sourcePath;
    error_->clear();
    refresh();
    emit changed();
}
bool SaveEditor::editDefaultArmy(const QByteArray &bytes, std::function<void(const QByteArray &)> sink) {
    if (!confirmDiscard()) return false;
    loadBytes(bytes);
    document_->selectSlot(0);
    defaultArmySink_ = std::move(sink);
    defaultArmy_->show();
    error_->setText("Editing the tester army. Click Use as Tester Army when finished.");
    tables_->setCurrentIndex(0);
    return true;
}
void SaveEditor::exportFile(const QString &path) {
    if (!document_)
        throw Error("No save loaded");
    if (pendingUnitEdits_)
        throw Error("Apply or discard the pending squad changes before exporting this save");
    if (!sourcePath_.isEmpty() &&
        QFileInfo(path).absoluteFilePath().compare(QFileInfo(sourcePath_).absoluteFilePath(),
                                                   Qt::CaseInsensitive) == 0)
        throw Error("Choose a different output file to preserve the loaded save");
    auto prepared = *document_;
    const auto refresh = prepared.planArmyRefresh(*project_);
    if (!refresh.spans.isEmpty())
        prepared.apply(refresh, true);
    prepared.validateEquipmentEdits(int(project_->records(Table::Items).size()));
    prepared.validateClassEdits(int(project_->records(Table::Classes).size()));
    prepared.validateUnitSupplyEdits(*project_);
    const auto bytes = prepared.serialize();
    const auto reopened =
        SaveDocument::open(project_->runtimeRom(), bytes, int(project_->records(Table::Classes).size()));
    if (reopened.serialize() != bytes)
        throw Error("Edited save failed reopening");
    atomicWrite(path, bytes);
    if (!refresh.spans.isEmpty())
        undo_->push(new SaveCommand(this, refresh, "refresh squad data for current classes"));
    undo_->setClean();
    error_->setText("Exported " + QFileInfo(path).fileName());
}
bool SaveEditor::edit(SaveTable table, int row, const QString &key, const QVariant &value) {
    if (!document_ || !allowImmediateEdit())
        return false;
    try {
        document_->setClassCount(int(project_->records(Table::Classes).size()));
        auto change =
            table == SaveTable::Characters && (key == "class" || key == "secondaryClass")
                ? document_->planCharacterClassEdit(row, key, value, project_->classData())
                : document_->planEdit(table, row, key, value);
        if (!change.spans.isEmpty())
            undo_->push(new SaveCommand(this, std::move(change), key));
        error_->clear();
        return true;
    } catch (const Error &e) {
        error_->setText(QString::fromUtf8(e.what()));
        return false;
    }
}
void SaveEditor::apply(const SaveChange &change, bool forward) {
    document_->apply(change, forward);
}
bool SaveEditor::editUnitSupplies(int index, const QVector<int> &supplies) {
    if (!document_ || !allowImmediateEdit())
        return false;
    try {
        auto change = document_->planUnitSupplies(index, supplies, *project_);
        if (!change.spans.isEmpty())
            undo_->push(new SaveCommand(this, std::move(change), "carried squad item"));
        error_->clear();
        return true;
    } catch (const Error &e) {
        error_->setText(QString::fromUtf8(e.what()));
        return false;
    }
}
bool SaveEditor::editUnit(int index, const QVector<int> &cells, int unity, int tactic) {
    if (!document_)
        return false;
    try {
        auto change = document_->planUnitEdit(index, cells, unity, tactic, project_->classData());
        if (!change.spans.isEmpty())
            undo_->push(new SaveCommand(this, std::move(change), "squad"));
        error_->clear();
        return true;
    } catch (const Error &e) {
        error_->setText(QString::fromUtf8(e.what()));
        return false;
    }
}
bool SaveEditor::createLegion(int unit) {
    if (!document_ || !allowImmediateEdit())
        return false;
    try {
        auto change = document_->planCreateLegion(unit, project_->classData());
        if (!change.spans.isEmpty())
            undo_->push(new SaveCommand(this, std::move(change), "create legion"));
        error_->clear();
        return true;
    } catch (const Error &e) {
        error_->setText(QString::fromUtf8(e.what()));
        return false;
    }
}
bool SaveEditor::joinLegion(int index, int unit) {
    if (!document_ || !allowImmediateEdit())
        return false;
    try {
        auto change = document_->planJoinLegion(index, unit, project_->classData());
        if (!change.spans.isEmpty())
            undo_->push(new SaveCommand(this, std::move(change), "join legion"));
        error_->clear();
        return true;
    } catch (const Error &e) {
        error_->setText(QString::fromUtf8(e.what()));
        return false;
    }
}
bool SaveEditor::detachLegionUnit(int index, int unit) {
    if (!document_ || !allowImmediateEdit())
        return false;
    try {
        auto change = document_->planDetachLegionUnit(index, unit);
        if (!change.spans.isEmpty())
            undo_->push(new SaveCommand(this, std::move(change), "detach squad from legion"));
        error_->clear();
        return true;
    } catch (const Error &e) {
        error_->setText(QString::fromUtf8(e.what()));
        return false;
    }
}
bool SaveEditor::disbandLegion(int index) {
    if (!document_ || !allowImmediateEdit())
        return false;
    try {
        auto change = document_->planDisbandLegion(index);
        if (!change.spans.isEmpty())
            undo_->push(new SaveCommand(this, std::move(change), "disband legion"));
        error_->clear();
        return true;
    } catch (const Error &e) {
        error_->setText(QString::fromUtf8(e.what()));
        return false;
    }
}
bool SaveEditor::disbandUnit(int index) {
    if (!document_ || !allowImmediateEdit())
        return false;
    try {
        auto change = document_->planDisbandUnit(index);
        if (!change.spans.isEmpty())
            undo_->push(new SaveCommand(this, std::move(change), "disband squad"));
        error_->clear();
        return true;
    } catch (const Error &e) {
        error_->setText(QString::fromUtf8(e.what()));
        return false;
    }
}
bool SaveEditor::createCharacter(int classId, const QString &name, int level) {
    if (!document_ || !allowImmediateEdit()) return false;
    try {
        document_->setClassCount(int(project_->records(Table::Classes).size()));
        const auto items = project_->itemData();
        auto change=document_->planCreateCharacter(classId,name,project_->classData(),QRandomGenerator::global()->generate(), level, &items);
        undo_->push(new SaveCommand(this, std::move(change), "create reserve character"));
        error_->clear();
        return true;
    } catch (const Error &e) {
        error_->setText(QString::fromUtf8(e.what()));
        return false;
    }
}
int SaveEditor::promptAddCharacter(bool formSquad) {
    if (!document_ || !allowImmediateEdit()) return -1;
    const int id = document_->freeCharacterSlot();
    if (id < 0) { error_->setText("No free character slot is available in this save."); return -1; }
    QDialog dialog(this);
    dialog.setObjectName("saveNewCharacterDialog");
    dialog.setWindowTitle(formSquad ? "Add squad with a new leader" : "Add reserve character");
    auto layout = new QVBoxLayout(&dialog);
    auto note = new QLabel(formSquad
        ? "Recruit a leader and form a new squad in one Undo step. Stats follow the game's initialization and level growth."
        : "Recruit a reserve character using the game's starting stats and level growth, including the current project's class and default equipment bonuses.");
    note->setWordWrap(true); layout->addWidget(note);
    auto name = new QLineEdit("Recruit");
    name->setObjectName("saveNewCharacterName"); name->setMaxLength(12);
    layout->addWidget(new QLabel("Name")); layout->addWidget(name);
    auto choices = new QComboBox;
    choices->setObjectName("saveNewCharacterClass");
    const auto classes = project_->classData();
    for (const auto &record : project_->records(Table::Classes)) {
        if (!record.id) continue;
        const int secondary = u8(classes.entries()[record.id].record, 69);
        if (formSquad && !classes.commandType(record.id, secondary ? secondary : record.id)) continue;
        choices->addItem(QString("%1 · %2").arg(record.id).arg(record.name.simplified()), record.id);
    }
    choices->setCurrentIndex(std::max(0, choices->findData(5)));
    layout->addWidget(new QLabel(formSquad ? "Leader class" : "Class")); layout->addWidget(choices);
    auto level = new QSpinBox;
    level->setObjectName("saveNewCharacterLevel");
    level->setRange(1, 99); level->setValue(1);
    layout->addWidget(new QLabel("Level")); layout->addWidget(level);
    auto cell = new QComboBox;
    cell->setObjectName("saveNewSquadCell");
    for (int i=0; i<9; ++i) cell->addItem(QString("Cell %1 · %2").arg(i+1).arg(i<3 ? "Rear" : i<6 ? "Middle" : "Front"),i);
    cell->setCurrentIndex(4);
    if (formSquad) {layout->addWidget(new QLabel("Starting position"));layout->addWidget(cell);}
    else cell->setParent(&dialog);
    cell->setVisible(formSquad);
    auto actions = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    actions->button(QDialogButtonBox::Ok)->setText(formSquad ? "Recruit and form squad" : "Create character");
    actions->button(QDialogButtonBox::Ok)->setEnabled(choices->count() > 0);
    if (!choices->count()) note->setText("No class in this project can lead a squad. Enable command capability in Classes first.");
    layout->addWidget(actions);
    connect(actions, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(actions, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return -1;
    const bool created = formSquad ? recruitUnit(choices->currentData().toInt(), name->text(), cell->currentData().toInt(), level->value())
                                   : createCharacter(choices->currentData().toInt(), name->text(), level->value());
    return created ? id : -1;
}
void SaveEditor::selectCharacter(int id) {
    if (!document_ || !allowImmediateEdit()) return;
    auto grid = findChild<QTableView *>("saveCharacters");
    findChild<QLineEdit *>("saveCharactersSearch")->clear();
    tables_->setCurrentIndex(0);
    for (int row=0; row<grid->model()->rowCount(); ++row)
        if (grid->model()->index(row,0).data().toInt()==id) {
            const auto index=grid->model()->index(row,1);
            grid->setCurrentIndex(index); grid->scrollTo(index,QAbstractItemView::PositionAtCenter);
            break;
        }
}
void SaveEditor::editCharacter(int id) {
    if (!document_ || !allowImmediateEdit() || id < 1 || id >= 100 ||
        !document_->rows(SaveTable::Characters).contains(id))
        return;
    QDialog dialog(this);
    dialog.setObjectName("saveMemberDialog");
    dialog.setWindowTitle("Edit member — " + document_->value(SaveTable::Characters, id, "name").toString());
    dialog.resize(760, 650);
    auto layout = new QVBoxLayout(&dialog);
    auto area = new QScrollArea;
    area->setWidgetResizable(true);
    auto body = new QWidget;
    auto columns = new QHBoxLayout(body);
    auto stats = new QGroupBox("Character and stats");
    auto equipment = new QGroupBox("Equipment");
    auto statForm = new QFormLayout(stats);
    auto gearForm = new QFormLayout(equipment);
    columns->addWidget(stats, 1);
    columns->addWidget(equipment, 1);
    columns->setAlignment(equipment, Qt::AlignTop);
    SaveModel model(this, SaveTable::Characters);
    QMap<QString, QWidget *> inputs;
    for (const auto &field : document_->fields(SaveTable::Characters)) {
        const auto value = document_->value(SaveTable::Characters, id, field.key);
        if (field.key == "class" || field.key == "secondaryClass") {
            const int classId = value.toInt();
            statForm->addRow(field.label, new QLabel(classId < project_->records(Table::Classes).size()
                ? QString(project_->record(Table::Classes, classId).name).replace('\n', ' ')
                : QString::number(classId)));
            continue;
        }
        QWidget *input;
        const auto choices = model.choices(field);
        if (!choices.isEmpty()) {
            auto box = new QComboBox;
            box->setMinimumContentsLength(16);
            box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            for (int i = int(field.minimum); i < choices.size(); ++i)
                box->addItem(choices[i].toString().replace('\n', ' '), i);
            if (box->findData(value) < 0)
                box->addItem(QString("Keep existing (%1)").arg(value.toInt()), value);
            box->setCurrentIndex(box->findData(value));
            input = box;
        } else if (field.key == "name") {
            auto text = new QLineEdit(value.toString());
            text->setMaxLength(std::max(field.bytes - 1, int(value.toString().size())));
            input = text;
        } else {
            auto number = new QSpinBox;
            number->setRange(std::min(int(field.minimum), value.toInt()),
                             std::max(int(field.maximum), value.toInt()));
            number->setValue(value.toInt());
            input = number;
        }
        input->setObjectName("saveMember_" + field.key);
        inputs[field.key] = input;
        const bool gear = field.key == "weapon" || field.key == "body" ||
                          field.key == "offhand" || field.key == "head";
        (gear ? gearForm : statForm)->addRow(field.label, input);
    }
    area->setWidget(body);
    layout->addWidget(area, 1);
    auto error = new QLabel;
    error->setObjectName("saveMemberError");
    error->setWordWrap(true);
    layout->addWidget(error);
    for (auto input : inputs) {
        if (auto box = qobject_cast<QComboBox *>(input))
            connect(box, &QComboBox::currentIndexChanged, error, &QLabel::clear);
        else if (auto text = qobject_cast<QLineEdit *>(input))
            connect(text, &QLineEdit::textChanged, error, &QLabel::clear);
        else
            connect(qobject_cast<QSpinBox *>(input), &QSpinBox::valueChanged, error, &QLabel::clear);
    }
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        try {
            auto draft = *document_;
            QVector<SaveChange> changes;
            auto setField = [&](const QString &key, const QVariant &value) {
                auto change = draft.planEdit(SaveTable::Characters, id, key, value);
                if (!change.spans.isEmpty()) {
                    draft.apply(change, true);
                    changes.append(std::move(change));
                }
            };
            // Raise the maximum before current HP; when lowering it, lower current HP first.
            const int maximumHp = qobject_cast<QSpinBox *>(inputs["hpMax"])->value();
            if (maximumHp > draft.value(SaveTable::Characters, id, "hpMax").toInt())
                setField("hpMax", maximumHp);
            for (auto it = inputs.cbegin(); it != inputs.cend(); ++it) {
                QVariant value;
                if (auto box = qobject_cast<QComboBox *>(it.value())) value = box->currentData();
                else if (auto text = qobject_cast<QLineEdit *>(it.value())) value = text->text();
                else value = qobject_cast<QSpinBox *>(it.value())->value();
                setField(it.key(), value);
            }
            // Validate the complete draft before publishing any of its fields.
            draft.serialize();
            if (!changes.isEmpty()) {
                undo_->beginMacro("Edit member stats and equipment");
                for (auto &change : changes)
                    undo_->push(new SaveCommand(this, std::move(change), "member field"));
                undo_->endMacro();
            }
            dialog.accept();
        } catch (const Error &e) {
            error->setText(QString::fromUtf8(e.what()));
        }
    });
    dialog.exec();
}
bool SaveEditor::recruitUnit(int classId, const QString &name, int cell, int level) {
    if (!document_ || !allowImmediateEdit()) return false;
    try {
        document_->setClassCount(int(project_->records(Table::Classes).size()));
        const auto items = project_->itemData();
        auto change = document_->planRecruitUnit(classId, name, cell, project_->classData(),
                                                  QRandomGenerator::global()->generate(), level, &items);
        undo_->push(new SaveCommand(this, std::move(change), "recruit leader and form squad"));
        error_->clear();
        return true;
    } catch (const Error &e) {error_->setText(QString::fromUtf8(e.what()));return false;}
}
bool SaveEditor::removeCharacter(int id) {
    if (!document_ || !allowImmediateEdit())
        return false;
    try {
        auto change = document_->planRemoveCharacter(id);
        undo_->push(new SaveCommand(this, std::move(change), "remove reserve character"));
        error_->clear();
        return true;
    } catch (const Error &e) {
        error_->setText(QString::fromUtf8(e.what()));
        return false;
    }
}
bool SaveEditor::createUnit(int leader, int cell) {
    if (!document_ || !allowImmediateEdit())
        return false;
    try {
        auto change = document_->planCreateUnit(leader, cell, project_->classData());
        if (!change.spans.isEmpty())
            undo_->push(new SaveCommand(this, std::move(change), "create squad"));
        error_->clear();
        return true;
    } catch (const Error &e) {
        error_->setText(QString::fromUtf8(e.what()));
        return false;
    }
}
bool SaveEditor::changeUnitLeader(int index, int member) {
    if (!document_ || !allowImmediateEdit())
        return false;
    try {
        auto change = document_->planUnitLeader(index, member, project_->classData());
        if (!change.spans.isEmpty())
            undo_->push(new SaveCommand(this, std::move(change), "change squad leader"));
        error_->clear();
        return true;
    } catch (const Error &e) {
        error_->setText(QString::fromUtf8(e.what()));
        return false;
    }
}
bool SaveEditor::swapUnitMembers(int fromUnit, int fromMember, int toUnit, int toMember) {
    if (!document_ || !allowImmediateEdit())
        return false;
    try {
        auto change =
            document_->planUnitSwap(fromUnit, fromMember, toUnit, toMember, project_->classData());
        if (!change.spans.isEmpty())
            undo_->push(new SaveCommand(this, std::move(change), "squad member swap"));
        error_->clear();
        return true;
    } catch (const Error &e) {
        error_->setText(QString::fromUtf8(e.what()));
        return false;
    }
}
bool SaveEditor::transferUnitMember(int fromUnit, int fromMember, int toUnit, int cell) {
    if (!document_ || !allowImmediateEdit())
        return false;
    try {
        auto change =
            document_->planUnitTransfer(fromUnit, fromMember, toUnit, cell, project_->classData());
        if (!change.spans.isEmpty())
            undo_->push(new SaveCommand(this, std::move(change), "squad member move"));
        error_->clear();
        return true;
    } catch (const Error &e) {
        error_->setText(QString::fromUtf8(e.what()));
        return false;
    }
}
bool SaveEditor::moveToReserve(int fromUnit, int fromMember) {
    if (!document_ || !allowImmediateEdit())
        return false;
    try {
        auto change = document_->planToReserve(fromUnit, fromMember, project_->classData());
        if (!change.spans.isEmpty())
            undo_->push(new SaveCommand(this, std::move(change), "move member to reserve"));
        error_->clear();
        return true;
    } catch (const Error &e) {
        error_->setText(QString::fromUtf8(e.what()));
        return false;
    }
}
bool SaveEditor::addFromReserve(int member, int toUnit, int cell) {
    if (!document_ || !allowImmediateEdit())
        return false;
    try {
        auto change = document_->planFromReserve(member, toUnit, cell, project_->classData());
        if (!change.spans.isEmpty())
            undo_->push(new SaveCommand(this, std::move(change), "add member from reserve"));
        error_->clear();
        return true;
    } catch (const Error &e) {
        error_->setText(QString::fromUtf8(e.what()));
        return false;
    }
}
bool SaveEditor::addSoldierGroup(int toUnit, int cell) {
    if (!document_ || !allowImmediateEdit())
        return false;
    try {
        auto change = document_->planSoldierGroup(toUnit, cell, project_->classData());
        if (!change.spans.isEmpty())
            undo_->push(new SaveCommand(this, std::move(change), "create Soldier group"));
        error_->clear();
        return true;
    } catch (const Error &e) {
        error_->setText(QString::fromUtf8(e.what()));
        return false;
    }
}
bool SaveEditor::allowImmediateEdit() {
    if (!pendingUnitEdits_)
        return true;
    error_->setText("Apply or discard the pending squad changes first");
    return false;
}
void SaveEditor::setUnitDraftPending(bool pending) {
    if (pendingUnitEdits_ == pending)
        return;
    pendingUnitEdits_ = pending;
    if (!pending)
        error_->clear();
    updateActions();
    emit changed();
}
void SaveEditor::updateActions() {
    export_->setEnabled(bool(document_) && !pendingUnitEdits_);
    restore_->setEnabled(undo_->index() > 0 && !pendingUnitEdits_);
    slots_->setEnabled(document_ && document_->battery() && !pendingUnitEdits_);
    undoButton_->setEnabled(undo_->canUndo() && !pendingUnitEdits_);
    redoButton_->setEnabled(undo_->canRedo() && !pendingUnitEdits_);
    tables_->setEnabled(bool(document_));
    for (int i = 0; i < tables_->count(); ++i)
        tables_->setTabEnabled(i, !pendingUnitEdits_ ||
                                      tables_->widget(i)->objectName() == "saveSquads");
}
void SaveEditor::refresh() {
    const ViewPosition keepView(this);
    const QSignalBlocker guard(slots_);
    slots_->clear();
    if (document_) {
        for (const auto &slot : document_->saveSlots()) {
            slots_->addItem(slot.valid ? slot.name : slot.name + " — " + slot.reason, slot.id);
            auto model = qobject_cast<QStandardItemModel *>(slots_->model());
            model->item(slots_->count() - 1)->setEnabled(slot.valid);
        }
        slots_->setCurrentIndex(slots_->findData(document_->selectedSlot()));
    }
    updateActions();
    status_->setText(
        document_
            ? QString("%1 · %2%3")
                  .arg(QFileInfo(sourcePath_).fileName(), document_->formatName(),
                       modified() ? " · Unsaved edits" : "")
            : "Open a Project64 SRAM, BizHawk SaveRAM, RetroArch state or native RDRAM file.");
    emit refreshed();
}
} // namespace lordly
