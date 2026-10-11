#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QComboBox;
class QSpinBox;
class QLabel;
class QPushButton;
namespace lordly {
class SquadOrderEditor : public QWidget {
  public:
    SquadOrderEditor(Project *, std::function<void(const Scenarios &, const QString &)>);
    void refresh(int key, int squad);
    void enableMapPicking(std::function<void(bool)>);
    void setDestination(QPoint);
    void cancelPick();
  protected:
    void hideEvent(QHideEvent *) override;
  private:
    void controls();
    Project *project_;
    std::function<void(const Scenarios &, const QString &)> commit_;
    std::function<void(bool)> picking_;
    QComboBox *command_, *destination_;
    QSpinBox *x_, *z_;
    QLabel *current_, *help_, *notice_;
    QPushButton *apply_, *pick_;
    QWidget *destinations_, *coordinates_;
    int key_ = -1, squad_ = -1;
};
}
