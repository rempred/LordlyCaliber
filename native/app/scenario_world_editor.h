#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QUndoStack;
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
    void selectObject(bool treasure, int row);

  private:
    void showSite();
    void showTreasure();
    void fillItems();
    int siteId() const;
    void editSite(const QString &, const std::function<void(QByteArray &)> &);
    void editTreasure(const QString &, const std::function<void(QByteArray &)> &);
    void setFacility(int bit, bool enabled);
    void setShop(int id);
    void change(const QString &label, const std::function<void(ScenarioWorld &)> &edit);
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    int key_ = -1;
    bool loading_ = false;
    int siteRow_ = -1, treasureRow_ = -1;
    QWidget *siteFields_, *treasureFields_;
    QLineEdit *name_;
    QSpinBox *population_, *morale_, *shop_, *x_, *z_;
    QComboBox *type_, *allegiance_, *table_, *item_;
    QCheckBox *objective_;
    QVector<QCheckBox *> facilities_;
    QMap<int, int> rememberedShops_;
    QLabel *notice_, *siteHelp_, *flag_;
};
} // namespace lordly
