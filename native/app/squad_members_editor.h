#pragma once
#include "engine/project.h"
#include "engine/squad_members.h"
#include <QWidget>
class QComboBox;
class QSpinBox;
class QTableWidget;
class QLabel;
namespace lordly {
class FormationBoard;
class SquadMembersEditor : public QWidget {
    Q_OBJECT
  public:
    SquadMembersEditor(Project *, const QString &prefix, int minimumMembers, int minimumClass,
                       QWidget *parent = nullptr);
    QVector<CustomMember> members() const;
    void setMembers(const QVector<CustomMember> &members);
    void setLevelEditable(int member, bool enabled);
  signals:
    void edited();
  private:
    void preview();
    void equipmentChoices(int member, const std::array<int, 2> &values);
    void moveMember(int member, int cell);
    void changeCount();
    Project *project_;
    int minimumClass_, shownCount_ = 0;
    bool loading_ = true;
    QSpinBox *count_;
    QTableWidget *table_;
    FormationBoard *board_;
    QLabel *notice_;
    QVector<QComboBox *> classes_, equipment_;
    QVector<QSpinBox *> levels_;
    QVector<int> positions_;
    QStringList classLabels_;
};
}
