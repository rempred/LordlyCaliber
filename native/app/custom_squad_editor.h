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
    void moveMember(int member, int cell);
    bool eventFilter(QObject *object, QEvent *event) override;
    CustomSquad collect() const;
    Project *project_;
    std::function<void(const Encounters &, const QString &)> commit_;
    QListWidget *list_;
    QLineEdit *search_, *label_;
    QWidget *details_;
    QComboBox *scenario_, *terrain_, *persuasion_;
    QCheckBox *enabled_;
    QSpinBox *count_, *chance_, *retreat_;
    QTableWidget *members_;
    QVector<QComboBox *> classes_, equipment_, bonusClasses_, rewards_;
    QVector<QSpinBox *> levels_, bonuses_, weights_;
    QVector<QLabel *> odds_;
    QVector<QPushButton *> cells_;
    QLabel *notice_, *formation_;
    QPushButton *copy_, *remove_, *restore_;
    QVector<int> positions_;
    int draggedMember_ = -1;
    QPoint dragStart_;
    int editing_ = -1;
    bool loading_ = true;
};
} // namespace lordly
