#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QUndoStack;
class QListWidget;
class QLineEdit;
class QComboBox;
class QSpinBox;
class QLabel;
class QTableWidget;
class QCheckBox;
namespace lordly {
class AttackEditor : public QWidget {
    Q_OBJECT
  public:
    AttackEditor(Project *, QUndoStack *, std::function<void()>, QWidget *parent = nullptr);
    void selectAttack(int);
  signals:
    void editAnimation(int classId, int attackId);

  private:
    void refresh();
    void filter();
    void showAttack();
    void edit(const QString &, const std::function<void(Attack &)> &);
    void publish(const AttackData &, const QString &);
    void rename();
    void create(bool copy);
    void showUsers();
    void assignClass();
    void preview();
    void editEffect();
    int selected() const;
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    bool refreshing_ = false;
    QListWidget *list_;
    QLineEdit *search_;
    QLabel *capacity_, *name_, *shared_, *relationship_, *elementHelp_, *targetHelp_,
        *nativeStatus_, *nativeAfter_, *advanced_, *notice_, *playback_;
    QWidget *details_, *effectPanel_, *amountPanel_, *conditionPanel_, *afterPanel_, *strikePanel_;
    QComboBox *element_, *effect_, *calculation_, *side_, *pattern_, *status_, *condition_, *after_,
        *basis_, *previewClass_, *playbackMode_;
    QSpinBox *power_, *chance_, *afterPercent_, *strikeCount_;
    QVector<QCheckBox *> cells_;
    QTableWidget *users_, *spellLinks_;
    QWidget *spellPanel_;
};
} // namespace lordly
