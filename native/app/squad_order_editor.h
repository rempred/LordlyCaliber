#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QComboBox;
class QSpinBox;
class QLabel;
class QPushButton;
namespace lordly {
class PendingEdits;
class SquadOrderEditor : public QWidget {
  public:
    SquadOrderEditor(Project *, std::function<void(const Scenarios &, const QString &)>);
    void refresh(int key, int squad);
    void enableMapPicking(std::function<void(bool)>);
    void setDestination(QPoint);
    void cancelPick();
    std::optional<SquadCommand> previewCommand() const;
    bool hasDraft() const;
    std::function<void()> stateChanged;
    std::function<void(int)> selectLeader;
  protected:
    void hideEvent(QHideEvent *) override;
  private:
    void controls();
    void edited(bool destinationReady = false);
    void applyCommand();
    bool applyDrafts();
    struct Draft { int command, destination, x, z; bool ready; QByteArray basis; };
    QMap<QPair<int,int>,Draft> drafts_;
    PendingEdits *pending_ = nullptr;
    QPair<int,int> identity_;
    QByteArray basis_;
    bool loading_ = false, complex_ = false, ready_ = false, follower_ = false;
    int leader_ = -1;
    Project *project_;
    std::function<void(const Scenarios &, const QString &)> commit_;
    std::function<void(bool)> picking_;
    QComboBox *command_, *destination_;
    QSpinBox *x_, *z_;
    QLabel *current_, *help_, *notice_;
    QPushButton *apply_, *pick_, *leaderButton_, *discard_;
    QWidget *destinations_, *coordinates_;
    int key_ = -1, squad_ = -1;
};
}
