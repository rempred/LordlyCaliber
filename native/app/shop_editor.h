#pragma once
#include "engine/project.h"
#include <QUndoStack>
#include <QWidget>
#include <functional>
class QListWidget;
class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;
namespace lordly {
// One Undo step owns both stock creation and the stronghold's facility assignment.
int addShop(Project *project, QUndoStack *undo, int townId = -1);
void assignShop(Project *project, QUndoStack *undo, int townId, int shopId);
class ShopEditor : public QWidget {
  public:
    ShopEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
               QWidget *parent = nullptr);

  private:
    struct Inventory {
        QListWidget *list = nullptr;
        QLabel *count = nullptr;
        QPushButton *add = nullptr, *remove = nullptr, *up = nullptr, *down = nullptr;
    };
    QWidget *makeInventory(bool equipment);
    QString itemName(bool equipment, int id) const;
    QStringList locations(int id) const;
    QVector<quint16> chooseItems(bool equipment);
    int selected() const;
    void refresh();
    void filter();
    void edit(const QString &label, const ShopStock &stock);
    void change(bool equipment, int operation);
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    QListWidget *shops_;
    QLineEdit *search_;
    QWidget *details_;
    QLabel *heading_, *notice_;
    QComboBox *copyFrom_, *town_;
    Inventory equipment_, consumables_;
};
} // namespace lordly
