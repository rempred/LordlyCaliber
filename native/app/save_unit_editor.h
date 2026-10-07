#pragma once
#include <QVector>
#include <QImage>
#include <QWidget>
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTabWidget;
namespace lordly {
class FormationBoard;
class SaveEditor;
class SaveUnitEditor final : public QWidget {
  public:
    explicit SaveUnitEditor(SaveEditor *editor);

  protected:
    void showEvent(QShowEvent *) override;

  private:
    void refresh();
    void showUnit();
    void drawFormation();
    void refreshSwapTargets();
    void refreshCreation(bool pending);
    QString memberName(int id) const;
    QImage memberSprite(int id) const;
    void selectLeader(int id);
    FormationBoard *board_;
    SaveEditor *editor_;
    QListWidget *units_, *members_, *reserves_;
    QTabWidget *detailTabs_;
    QLabel *armyCounts_, *funds_, *creationNote_;
    QPushButton *addCharacter_, *recruitSquad_;
    QVector<QPushButton *> grid_;
    QSpinBox *unity_;
    QComboBox *tactic_, *newLeader_, *newCell_;
    QPushButton *create_, *disband_, *disbandLegion_, *detachLegion_, *joinLegion_, *createLegion_;
    QComboBox *legionTarget_;
    QLabel *legion_;
    int shownLegion_ = -1;
    QComboBox *targetUnit_, *targetMember_;
    QComboBox *reserveMember_, *reserveCell_;
    QPushButton *toReserve_, *fromReserve_, *addSoldier_;
    QLabel *soldierPool_;
    QPushButton *swap_, *leader_;
    QPushButton *apply_, *discard_;
    QLabel *draftNote_;
    int shownUnit_ = -1;
    QLabel *note_, *capacity_;
    QVector<QComboBox *> supplies_;
    void refreshSupplies(bool pending);
    QVector<int> cells_, ids_;
    bool loading_ = false;
};
} // namespace lordly
