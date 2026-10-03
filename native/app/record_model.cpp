#include "app/record_model.h"
#include <QBrush>
#include <QColor>
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
        : QUndoCommand("Rename class"), model_(model), row_(row), before_(std::move(before)),
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
RecordModel::RecordModel(Project *p, Table t, QUndoStack *u, QObject *parent)
    : QAbstractTableModel(parent), project_(p), table_(t), undo_(u) {}
int RecordModel::rowCount(const QModelIndex &p) const {
    return p.isValid() ? 0 : int(project_->records(table_).size());
}
int RecordModel::columnCount(const QModelIndex &p) const {
    return p.isValid() ? 0 : int(fields(table_).size()) + 2;
}
QVariant RecordModel::data(const QModelIndex &i, int role) const {
    if (!i.isValid())
        return {};
    const auto &r = project_->record(table_, i.row());
    if (role == Qt::ForegroundRole &&
        (r.bytes != r.original ||
         (table_ == Table::Classes && project_->classNameModified(i.row()))))
        return QBrush(QColor("#e9bf67"));
    if (role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::ToolTipRole)
        return {};
    if (i.column() == 0)
        return role == Qt::DisplayRole
                   ? QVariant(QString("%1").arg(r.id, 2, 16, QChar('0')).toUpper())
                   : QVariant(r.id);
    if (i.column() == 1)
        return r.name;
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
        if (!label.isEmpty())
            return QString("%1 · %2").arg(value).arg(label);
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
    return n >= 2 && n < columnCount() ? fields(table_)[n - 2].label : QVariant();
}
Qt::ItemFlags RecordModel::flags(const QModelIndex &i) const {
    auto result = QAbstractTableModel::flags(i);
    if (i.column() == 1 && table_ == Table::Classes && project_->hasClassData())
        result |= Qt::ItemIsEditable;
    if (i.column() >= 2 && !(table_ == Table::Classes && project_->hasClassData() &&
                             fields(table_)[i.column() - 2].offset < 4))
        result |= Qt::ItemIsEditable;
    return result;
}
bool RecordModel::setData(const QModelIndex &i, const QVariant &v, int role) {
    if (role != Qt::EditRole || !i.isValid() || !(flags(i) & Qt::ItemIsEditable))
        return false;
    if (i.column() == 1) {
        if (v.toString() == project_->record(table_, i.row()).name)
            return false;
        try {
            const auto name = encodeClassName(v.toString());
            undo_->push(new EditClassName(this, i.row(), project_->classNameBytes(i.row()), name));
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
    project_->setClassNameBytes(row, value);
    // Growth/class references elsewhere in the grid display these names too.
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    emit edited();
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
