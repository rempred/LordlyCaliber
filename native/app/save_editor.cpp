#include "app/save_editor.h"
#include "app/save_unit_editor.h"
#include <QAbstractTableModel>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
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
                    record.id ? record.name : QString("Default / none")));
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
        connect(search, &QLineEdit::textChanged, filter,
                &QSortFilterProxyModel::setFilterFixedString);
        tables_->addTab(page, title);
    }
    tables_->addTab(new SaveUnitEditor(this), "Squads");
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
        SaveDocument::open(project_->rom(), bytes, int(project_->records(Table::Classes).size()));
    pendingUnitEdits_ = false; // The caller has already handled replacing the loaded document.
    undo_->clear();
    document_ = std::move(candidate);
    sourcePath_ = sourcePath;
    error_->clear();
    refresh();
    emit changed();
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
    document_->validateClassEdits(int(project_->records(Table::Classes).size()));
    document_->validateUnitSupplyEdits(*project_);
    const auto bytes = document_->serialize();
    const auto reopened =
        SaveDocument::open(project_->rom(), bytes, int(project_->records(Table::Classes).size()));
    if (reopened.serialize() != bytes)
        throw Error("Edited save failed reopening");
    atomicWrite(path, bytes);
    undo_->setClean();
    error_->setText("Exported " + QFileInfo(path).fileName());
}
bool SaveEditor::edit(SaveTable table, int row, const QString &key, const QVariant &value) {
    if (!document_ || !allowImmediateEdit())
        return false;
    try {
        document_->setClassCount(int(project_->records(Table::Classes).size()));
        auto change = document_->planEdit(table, row, key, value);
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
