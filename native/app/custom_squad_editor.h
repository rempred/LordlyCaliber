#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QUndoStack;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QLineEdit;
class QListWidget;
class QLabel;
class QPushButton;
class QTableWidget;
namespace lordly {
class SquadMembersEditor;
class CustomSquadEditor : public QWidget {
  public:
    CustomSquadEditor(Project *project, QUndoStack *undo,
                      std::function<void(const Encounters &, const QString &)> commit,
                      QWidget *parent = nullptr);

  private:
    void refresh();
    void filter();
    void showSelected();
    void display(const CustomSquad &squad);
    void newDraft(bool copy);
    void apply();
    void remove(bool restore);
    void preview();
    CustomSquad collect() const;
    SquadMembersEditor *memberEditor_;
    Project *project_;
    std::function<void(const Encounters &, const QString &)> commit_;
    QListWidget *list_;
    QLineEdit *search_, *label_;
    QWidget *details_;
    QComboBox *scenario_, *terrain_, *persuasion_;
    QCheckBox *enabled_;
    QSpinBox *chance_, *retreat_;
    QVector<QComboBox *> bonusClasses_, rewards_;
    QVector<QSpinBox *> bonuses_, weights_;
    QVector<QLabel *> odds_;
    QLabel *notice_;
    QPushButton *copy_, *remove_, *restore_;
    int editing_ = -1;
    bool loading_ = true;
};
} // namespace lordly
