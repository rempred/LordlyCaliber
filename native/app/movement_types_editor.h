#pragma once
#include "engine/project.h"
#include <QUndoStack>
#include <QWidget>
#include <functional>
#include <optional>
class QListWidget;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;
class QTableWidget;
namespace lordly {
struct MovementCreation {
    QString name;
    int copyFrom = 0, baseSpeed = 35;
};
std::optional<MovementCreation> requestMovementType(QWidget *, const Project &, int copyFrom = 2);
class MovementTypesEditor : public QWidget {
  public:
    MovementTypesEditor(Project *, QUndoStack *, std::function<void()>, QWidget *parent = nullptr);

  private:
    void refresh();
    void edit(const QString &, const std::function<void(MovementType &)> &);
    void apply(const QString &, const MovementTypes &);
    int selected() const;
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    QListWidget *list_;
    QLineEdit *name_;
    QSpinBox *speed_;
    QComboBox *profile_;
    QMap<QString,QCheckBox *> switches_;
    QMap<QString,QSpinBox *> rates_;
    QLabel *formula_;
    QTableWidget *terrain_;
};
} // namespace lordly
