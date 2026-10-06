#pragma once
#include <QVector>
#include <QWidget>
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
namespace lordly {
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
    SaveEditor *editor_;
    QListWidget *units_, *members_;
    QVector<QPushButton *> grid_;
    QSpinBox *unity_;
    QComboBox *tactic_, *newLeader_, *newCell_;
    QPushButton *create_, *disband_, *disbandLegion_, *detachLegion_, *joinLegion_;
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
