#pragma once
#include "engine/project.h"
#include <QAbstractTableModel>
#include <QStyledItemDelegate>
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
    void applyName(int row, const QByteArray &value);
    int appendClass(int templateId, const QString &name);
    int createClass(const QString &name);
    int appendItem(int source, const QString &name);
    void applyProject(const Project &project);
  signals:
    void edited();
    void editRejected(const QString &message);

  private:
    Project *project_;
    Table table_;
    QUndoStack *undo_;
};
class ClassNameDelegate : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget *createEditor(QWidget *, const QStyleOptionViewItem &,
                          const QModelIndex &) const override;
    void setEditorData(QWidget *, const QModelIndex &) const override;
    void setModelData(QWidget *, QAbstractItemModel *, const QModelIndex &) const override;
    void updateEditorGeometry(QWidget *, const QStyleOptionViewItem &,
                              const QModelIndex &) const override;
    bool eventFilter(QObject *, QEvent *) override;
};
} // namespace lordly
