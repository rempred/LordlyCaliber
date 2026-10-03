#include "app/record_model.h"
#include <QBrush>
#include <QColor>
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
    if (role == Qt::ForegroundRole && r.bytes != r.original)
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
    if (i.column() >= 2)
        result |= Qt::ItemIsEditable;
    return result;
}
bool RecordModel::setData(const QModelIndex &i, const QVariant &v, int role) {
    if (role != Qt::EditRole || !i.isValid() || i.column() < 2)
        return false;
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
} // namespace lordly
