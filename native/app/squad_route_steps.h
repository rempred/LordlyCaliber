#pragma once
#include "engine/project.h"
#include "engine/squad_route.h"
#include <QWidget>
#include <optional>
#include <functional>
class QComboBox;
class QSpinBox;
class QListWidget;
class QLabel;
class QPushButton;
class QCheckBox;
namespace lordly {
class SquadTriggerEditor;
class SquadRouteSteps : public QWidget {
  public:
    SquadRouteSteps(Project *, std::function<void(const Scenarios &, const QString &)>, QWidget *parent = nullptr);
    void refresh(int key, int squad);
    void selectNode(int row);
    void setDestination(QPoint);
    void cancelPick();
    bool isPicking() const;
    bool isPickingArea() const { return areaSlot_ >= 0; }
    QString pickPrompt() const;
    QVector<QPair<QString, QRectF>> triggerAreas() const;
    const SquadRoute &route() const { return route_; }
    int selectedNode() const;
    std::function<void(bool)> picking;
    std::function<void()> stateChanged;
    std::function<void(int)> advanced;
    std::function<void(int)> selectLeader;
  protected:
    void hideEvent(QHideEvent *) override;
  private:
    void showStep();
    void edit(const QString &, const std::function<void(QByteArray &)> &);
    void change(const QString &, const std::function<void(Scenarios &)> &);
    void transition();
    void addStep();
    QByteArray defaultTrigger(int kind) const;
    void writeTrigger(int slot, const QByteArray &, bool create = false);
    QString destination(int x, int z) const;
    QString nextName(int id) const;
    QString continueName(const QByteArray &) const;
    Project *project_;
    std::function<void(const Scenarios &, const QString &)> commit_;
    SquadRoute route_;
    int key_ = -1, squad_ = -1;
    bool loading_ = false;
    QMap<QPair<int,int>,int> selections_;
    QListWidget *steps_;
    QWidget *details_, *place_, *coordinates_, *conditions_, *branch_;
    QComboBox *action_, *destination_, *when_, *conditionA_, *conditionB_, *next_, *fork_;
    SquadTriggerEditor *triggers_[2];
    QWidget *triggerGroups_[2];
    int areaSlot_ = -1;
    std::optional<QPoint> areaCorner_;
    QSpinBox *x_, *z_;
    QLabel *notice_, *scope_, *heading_;
    QPushButton *pick_, *add_, *advanced_, *leader_;
    QCheckBox *shared_;
};
}
