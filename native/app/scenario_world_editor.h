#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QUndoStack;
class QListWidget;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;
namespace lordly {
class ScenarioWorldEditor : public QWidget {
  public:
    ScenarioWorldEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                        QWidget *parent = nullptr);
    void refresh(int key);

  private:
    void showSite();
    void showTreasure();
    void fillItems();
    void change(const QString &label, const std::function<void(ScenarioWorld &)> &edit);
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    int key_ = -1;
    bool loading_ = false;
    QListWidget *sites_, *treasures_;
    QWidget *siteFields_, *treasureFields_;
    QLineEdit *name_;
    QSpinBox *population_, *morale_, *shop_, *x_, *z_;
    QComboBox *type_, *allegiance_, *table_, *item_;
    QCheckBox *objective_;
    QVector<QCheckBox *> facilities_;
    QLabel *notice_, *siteHelp_, *flag_;
};
} // namespace lordly
