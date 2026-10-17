#include "app/record_model.h"
#include "engine/weapon_status.h"
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QUndoCommand>

namespace lordly {
class EditField final : public QUndoCommand {
  public:
    EditField(RecordModel *model, int row, int field, int before, int after, QString text)
        : QUndoCommand(text), model_(model), row_(row), field_(field), before_(before),
          after_(after) {}
    void undo() override {
        model_->apply(row_, field_, before_);
    }
    void redo() override {
        model_->apply(row_, field_, after_);
    }

  private:
    RecordModel *model_;
    int row_, field_, before_, after_;
};
class EditClassName final : public QUndoCommand {
  public:
    EditClassName(RecordModel *model, int row, QByteArray before, QByteArray after)
        : QUndoCommand("Rename record"), model_(model), row_(row), before_(std::move(before)),
          after_(std::move(after)) {}
    void undo() override {
        model_->applyName(row_, before_);
    }
    void redo() override {
        model_->applyName(row_, after_);
    }

  private:
    RecordModel *model_;
    int row_;
    QByteArray before_, after_;
};
class AddClass final : public QUndoCommand {
  public:
    AddClass(RecordModel *model, Project before, Project after, QString label = "Add class")
        : QUndoCommand(label), model_(model), before_(std::move(before)), after_(std::move(after)) {
    }
    void undo() override {
        model_->applyProject(before_);
    }
    void redo() override {
        model_->applyProject(after_);
    }

  private:
    RecordModel *model_;
    Project before_, after_;
};
RecordModel::RecordModel(Project *p, Table t, QUndoStack *u, QObject *parent)
    : QAbstractTableModel(parent), project_(p), table_(t), undo_(u) {}
int RecordModel::rowCount(const QModelIndex &p) const {
    return p.isValid() ? 0 : int(project_->records(table_).size());
}
int RecordModel::columnCount(const QModelIndex &p) const {
    return p.isValid() ? 0 : int(fields(table_).size()) + 2 + (table_ == Table::Items ? 1 : 0);
}
int RecordModel::weaponStatusColumn() const {
    return table_ == Table::Items ? int(fields(table_).size()) + 2 : -1;
}
QVariant RecordModel::data(const QModelIndex &i, int role) const {
    if (!i.isValid())
        return {};
    const auto &r = project_->record(table_, i.row());
    if (role == Qt::ForegroundRole &&
        (r.bytes != r.original ||
         (table_ == Table::Classes && project_->classNameModified(i.row())) ||
         (table_ == Table::Items && project_->hasItemData() &&
          project_->itemNameModified(i.row()))))
        return QBrush(QColor("#982018"));
    if (role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::ToolTipRole)
        return {};
    if (i.column() == 0)
        return role == Qt::DisplayRole
                   ? QVariant(QString("%1").arg(r.id, 2, 16, QChar('0')).toUpper())
                   : QVariant(r.id);
    if (i.column() == 1 && role == Qt::EditRole && table_ == Table::Classes && project_->hasClassData())
        return decodeText(project_->classNameBytes(i.row()));
    if (i.column() == 1)
        return role == Qt::DisplayRole ? r.name.simplified() : r.name;
    if (i.column() == weaponStatusColumn()) {
        const bool weapon = r.id > 0 && WeaponStatus::isWeapon(r.bytes);
        const int status = u8(r.bytes, WeaponStatus::offset) & WeaponStatus::mask;
        if (role == Qt::ToolTipRole)
            return weapon ? "Status applied by this weapon's physical hits. Double-click to change."
                          : "On-hit status effects apply to weapons, not armor or accessories.";
        if (role == Qt::EditRole) return weapon ? status : -1;
        return weapon ? WeaponStatus::name(status) : QString("—");
    }
    const auto &f = fields(table_)[i.column() - 2];
    int value = project_->value(table_, i.row(), i.column() - 2);
    if (role == Qt::ToolTipRole)
        return QString("%1 · %2 to %3\nStored offset %4 in the current record")
            .arg(f.group)
            .arg(f.minimum())
            .arg(f.maximum())
            .arg(f.offset);
    if (role == Qt::DisplayRole && !f.choices.isEmpty()) {
        auto names = project_->catalog().value(f.choices).toObject();
        auto label = names.value(QString::number(value)).toString();
        if (f.key == "classCopyMatch" && value == 0) label = "Same as this class";
        if (!label.isEmpty())
            return QString("%1 · %2").arg(value).arg(label.simplified());
    }
    return value;
}
QVariant RecordModel::headerData(int n, Qt::Orientation o, int role) const {
    if (role != Qt::DisplayRole)
        return {};
    if (o == Qt::Vertical)
        return n + 1;
    if (n == 0)
        return "ID";
    if (n == 1)
        return "Name";
    if (n >= 2 && n == weaponStatusColumn())
        return "Status effect on hit";
    return n >= 2 && n < columnCount() ? fields(table_)[n - 2].label : QVariant();
}
Qt::ItemFlags RecordModel::flags(const QModelIndex &i) const {
    auto result = QAbstractTableModel::flags(i);
    if (!i.isValid()) return result;
    if (i.column() == weaponStatusColumn()) {
        if (i.row() > 0 && WeaponStatus::isWeapon(project_->record(table_, i.row()).bytes))
            result |= Qt::ItemIsEditable;
        return result;
    }
    if (i.column() == 1 && ((table_ == Table::Classes && project_->hasClassData()) ||
                            (table_ == Table::Items && project_->hasItemData())))
        result |= Qt::ItemIsEditable;
    if (i.column() >= 2 && !(((table_ == Table::Classes && project_->hasClassData()) ||
                              (table_ == Table::Items && project_->hasItemData())) &&
                             fields(table_)[i.column() - 2].offset < 4))
        result |= Qt::ItemIsEditable;
    return result;
}
bool RecordModel::setData(const QModelIndex &i, const QVariant &v, int role) {
    if (role != Qt::EditRole || !i.isValid() || !(flags(i) & Qt::ItemIsEditable))
        return false;
    if (i.column() == 1) {
        if (v.toString() == data(i, Qt::EditRole).toString())
            return false;
        try {
            const auto name = table_ == Table::Items ? encodeItemName(v.toString())
                                                     : encodeClassName(v.toString());
            undo_->push(new EditClassName(this, i.row(),
                                          table_ == Table::Items
                                              ? project_->itemNameBytes(i.row())
                                              : project_->classNameBytes(i.row()),
                                          name));
            return true;
        } catch (const Error &error) {
            emit editRejected(QString::fromUtf8(error.what()));
            return false;
        }
    }
    bool ok = false;
    int value = v.toInt(&ok);
    if (!ok)
        return false;
    if (i.column() == weaponStatusColumn()) {
        const int before = u8(project_->record(table_, i.row()).bytes, WeaponStatus::offset);
        if ((before & WeaponStatus::mask) == value) return false;
        try {
            const int next = WeaponStatus::replace(before, value);
            for (int field = 0; field < fields(table_).size(); ++field)
                if (fields(table_)[field].key == "b23Raw") {
                    undo_->push(new EditField(this, i.row(), field, before, next, "Edit status effect on hit"));
                    return true;
                }
        } catch (const Error &error) {
            emit editRejected(QString::fromUtf8(error.what()));
        }
        return false;
    }
    const auto &f = fields(table_)[i.column() - 2];
    if (value < f.minimum() || value > f.maximum())
        return false;
    int before = project_->value(table_, i.row(), i.column() - 2);
    if (before == value)
        return false;
    undo_->push(new EditField(this, i.row(), i.column() - 2, before, value, "Edit " + f.label));
    return true;
}
void RecordModel::apply(int row, int field, int value) {
    project_->setValue(table_, row, field, value);
    emit dataChanged(index(row, 0), index(row, columnCount() - 1));
    emit edited();
}
void RecordModel::applyName(int row, const QByteArray &value) {
    if (table_ == Table::Items)
        project_->setItemNameBytes(row, value);
    else
        project_->setClassNameBytes(row, value);
    // Growth/class references elsewhere in the grid display these names too.
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    emit edited();
}
int RecordModel::appendClass(int templateId, const QString &name) {
    try {
        if (table_ != Table::Classes)
            throw Error("New classes require the Classes workspace");
        auto after = *project_;
        const auto id = after.appendClass(templateId, name);
        undo_->push(new AddClass(this, *project_, std::move(after)));
        return id;
    } catch (const Error &error) {
        emit editRejected(QString::fromUtf8(error.what()));
        return -1;
    }
}
int RecordModel::createClass(const QString &name) {
    try {
        if (table_ != Table::Classes)
            throw Error("New classes require the Classes workspace");
        auto after = *project_;
        const auto id = after.createClass(name);
        undo_->push(new AddClass(this, *project_, std::move(after)));
        return id;
    } catch (const Error &error) {
        emit editRejected(QString::fromUtf8(error.what()));
        return -1;
    }
}
int RecordModel::appendItem(int source, const QString &name) {
    try {
        if (table_ != Table::Items)
            throw Error("New items require the Items workspace");
        auto after = *project_;
        const int id = after.appendItem(source, name);
        undo_->push(new AddClass(this, *project_, std::move(after), "Add item"));
        return id;
    } catch (const Error &error) {
        emit editRejected(QString::fromUtf8(error.what()));
        return -1;
    }
}
int RecordModel::createItem(const QString &name) {
    try {
        if (table_ != Table::Items)
            throw Error("New items require the Items workspace");
        auto after = *project_;
        const int id = after.createItem(name);
        undo_->push(new AddClass(this, *project_, std::move(after), "Create item"));
        return id;
    } catch (const Error &error) {
        emit editRejected(QString::fromUtf8(error.what()));
        return -1;
    }
}
void RecordModel::applyProject(const Project &project) {
    emit aboutToReplace();
    beginResetModel();
    *project_ = project;
    endResetModel();
    emit replaced();
    emit edited();
}
QWidget *WeaponStatusDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &,
                                            const QModelIndex &) const {
    auto choice = new QComboBox(parent);
    for (int value : {0, WeaponStatus::poison, 2, 4, 8, WeaponStatus::powerUp, WeaponStatus::powerDown})
        choice->addItem(WeaponStatus::name(value), value);
    return choice;
}
void WeaponStatusDelegate::setEditorData(QWidget *widget, const QModelIndex &index) const {
    auto choice = static_cast<QComboBox *>(widget);
    const int status = index.data(Qt::EditRole).toInt();
    if (choice->findData(status) < 0)
        choice->addItem("Existing combination: " + WeaponStatus::name(status), status);
    choice->setCurrentIndex(choice->findData(status));
}
void WeaponStatusDelegate::setModelData(QWidget *widget, QAbstractItemModel *model,
                                       const QModelIndex &index) const {
    model->setData(index, static_cast<QComboBox *>(widget)->currentData(), Qt::EditRole);
}
QWidget *ClassNameDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &,
                                         const QModelIndex &) const {
    auto editor = new QPlainTextEdit(parent);
    editor->setTabChangesFocus(true);
    editor->setToolTip("One or two lines. Ctrl+Enter applies the name; Escape cancels.");
    return editor;
}
void ClassNameDelegate::setEditorData(QWidget *widget, const QModelIndex &index) const {
    static_cast<QPlainTextEdit *>(widget)->setPlainText(index.data(Qt::EditRole).toString());
}
void ClassNameDelegate::setModelData(QWidget *widget, QAbstractItemModel *model,
                                     const QModelIndex &index) const {
    model->setData(index, static_cast<QPlainTextEdit *>(widget)->toPlainText(), Qt::EditRole);
}
void ClassNameDelegate::updateEditorGeometry(QWidget *widget, const QStyleOptionViewItem &option,
                                             const QModelIndex &) const {
    auto rect = option.rect;
    rect.setHeight(qMax(rect.height(), 64));
    widget->setGeometry(rect);
}
bool ClassNameDelegate::eventFilter(QObject *object, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        const auto key = static_cast<QKeyEvent *>(event);
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) &&
            (key->modifiers() & Qt::ControlModifier)) {
            auto widget = qobject_cast<QWidget *>(object);
            emit commitData(widget);
            emit closeEditor(widget);
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
            return false;
    }
    return QStyledItemDelegate::eventFilter(object, event);
}
} // namespace lordly
