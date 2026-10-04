#pragma once
#include "engine/project.h"
#include <QUndoStack>
#include <QWidget>
#include <functional>
class QListWidget;
class QLineEdit;
class QLabel;
class QComboBox;
class QSpinBox;
namespace lordly {
class EncounterEditor : public QWidget {
  public:
    EncounterEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                    QWidget *parent = nullptr);

  private:
    void refresh();
    void filter();
    void changePair(int slot);
    void changeTerrain(int terrain);
    void commit(const Encounters &next, const QString &label);
    int selected() const;
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    QListWidget *pools_;
    QLineEdit *search_;
    QLabel *heading_, *notice_;
    QComboBox *copyFrom_;
    QVector<QComboBox *> pairs_, lookups_;
    QVector<QSpinBox *> rates_;
};
} // namespace lordly
