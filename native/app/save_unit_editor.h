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
class SaveArmyDrag;
class SaveCharacterCard;
class SaveUnitEditor final : public QWidget {
    Q_OBJECT
  public:
    explicit SaveUnitEditor(SaveEditor *editor);
    void selectUnit(int index);
    void selectCharacter(int id);

  protected:
    void showEvent(QShowEvent *) override;
    bool eventFilter(QObject *, QEvent *) override;

  private:
    void refresh();
    void showUnit();
    void drawFormation();
    void refreshControls();
    void refreshCreation();
    void commitFormation();
    void moveCell(int from, int to);
    QString memberName(int id) const;
    QImage memberSprite(int id) const;
    void selectLeader(int id);
    void startMemberDrag(int unit, int member, int id);
    bool dropMember(const SaveArmyDrag *, QObject *target, QPoint position, bool apply);
    FormationBoard *board_;
    SaveEditor *editor_;
    QListWidget *units_, *legions_, *members_, *reserves_;
    SaveCharacterCard *card_;
    bool formingSquad_ = false;
    QTabWidget *detailTabs_;
    QLabel *armyCounts_, *funds_, *creationNote_;
    QPushButton *addCharacter_, *recruitSquad_;
    QVector<QPushButton *> grid_;
    QSpinBox *unity_;
    QComboBox *tactic_;
    QPushButton *create_, *disband_, *disbandLegion_, *detachLegion_, *joinLegion_, *createLegion_;
    QComboBox *legionTarget_;
    QLabel *legion_;
    int shownLegion_ = -1;
    QPushButton *leader_;
    int shownUnit_ = -1;
    QLabel *note_, *capacity_;
    QVector<QComboBox *> supplies_;
    void refreshSupplies();
    QVector<int> cells_, ids_;
    bool loading_ = false;
};
} // namespace lordly
