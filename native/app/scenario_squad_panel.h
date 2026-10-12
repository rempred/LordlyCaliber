#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QLabel;
class QComboBox;
class QPushButton;
namespace lordly {
class FormationBoard;
class ScenarioSquadPanel : public QWidget {
  public:
    ScenarioSquadPanel(Project *, std::function<void(const Scenarios &, const QString &)>,
                       const QString &prefix, QWidget *parent = nullptr);
    void refresh(int key, int row);
    void editMembers();
  private:
    void changeDeployment();
    void moveMember(int from, int to);
    Project *project_;
    std::function<void(const Scenarios &, const QString &)> commit_;
    QString prefix_;
    FormationBoard *board_;
    QLabel *summary_, *notice_;
    QComboBox *side_, *facing_, *healing_, *fruit_, *revival_, *drop_;
    QPushButton *edit_, *restore_;
    int key_ = -1, row_ = -1;
    bool loading_ = false;
};
}
