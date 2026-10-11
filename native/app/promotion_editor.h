#pragma once
#include "engine/project.h"
#include <QUndoStack>
#include <QWidget>
#include <functional>
class QSpinBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QCheckBox;
class QListWidget;
namespace lordly {
class PromotionEditor : public QWidget {
  public:
    PromotionEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                    QWidget *parent = nullptr);
    void selectClass(int id);

  private:
    void refresh();
    void edit(const QString &label, const std::function<void(Promotion &)> &operation);
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    int id_ = -1;
    bool refreshing_ = false;
    QVector<QSpinBox *> gates_;
    QSpinBox *mask_;
    QCheckBox *equipmentSets_;
    QComboBox *families_, *rules_;
    QLineEdit *name_;
    QLabel *notice_, *linksTitle_;
    QListWidget *links_;
};
} // namespace lordly
