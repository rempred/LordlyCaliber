#pragma once
#include "engine/project.h"
#include <QAbstractTableModel>
#include <QUndoStack>

namespace lordly {
class RecordModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    RecordModel(Project *project, Table table, QUndoStack *undo, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    void apply(int row, int field, int value);
  signals:
    void edited();

  private:
    Project *project_;
    Table table_;
    QUndoStack *undo_;
};
} // namespace lordly
