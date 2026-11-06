#pragma once
#include "engine/project.h"
#include <QUndoStack>
#include <QWidget>
#include <functional>
class QLabel;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
namespace lordly {
class PendingEdits;
class ConsumableEditor : public QWidget {
  public:
    ConsumableEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                     QWidget *parent = nullptr);
    void selectConsumable(int id);
    void setAdvancedVisible(bool visible);

  private:
    void refresh();
    void openTree(const QList<int> &path);
    bool moveEffect(QTreeWidgetItem *effect, QTreeWidgetItem *branch);
    void commitTree(const QJsonArray &tree);
    void apply(bool restore);
    void rememberDraft();
    bool applyDrafts();
    struct Draft { int id; QJsonObject source, values; bool description; };
    QMap<QString, Draft> drafts_;
    PendingEdits *pending_;
    bool refreshing_ = false;
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    int selected_ = -1;
    QJsonObject definition_;
    QLabel *heading_, *target_, *notice_, *minimumLabel_, *maximumLabel_;
    QSpinBox *minimum_, *maximum_;
    QCheckBox *description_;
    QPushButton *apply_, *restore_, *discard_;
    QTreeWidget *tree_;
    QPushButton *editTree_, *restoreTree_;
    QPushButton *editVisual_, *restoreVisual_;
    QLabel *visualName_;
    QWidget *originalSettings_;
    QLabel *originalNote_;
    bool advanced_ = false;
};
} // namespace lordly
