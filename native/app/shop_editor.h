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
class ShopEditor : public QWidget {
  public:
    ShopEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
               QWidget *parent = nullptr);

  private:
    struct Inventory {
        QListWidget *list = nullptr;
        QComboBox *choice = nullptr;
        QLabel *count = nullptr;
        QPushButton *add = nullptr, *remove = nullptr, *up = nullptr, *down = nullptr;
    };
    QWidget *makeInventory(bool equipment);
    QString itemName(bool equipment, int id) const;
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
    QComboBox *copyFrom_;
    Inventory equipment_, consumables_;
};
} // namespace lordly
