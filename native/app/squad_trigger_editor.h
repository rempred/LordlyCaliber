#pragma once
#include "engine/eset.h"
#include <QWidget>
#include <functional>
class QComboBox;
class QSpinBox;
class QPushButton;
class QLabel;
namespace lordly {
class SquadTriggerEditor : public QWidget {
  public:
    explicit SquadTriggerEditor(const QString &suffix, QWidget *parent = nullptr);
    void refresh(const Eset &, int id, bool privateTrigger);
    void setPicking(bool on, bool second = false);
    std::function<void(int)> selected;
    std::function<void(int, bool)> create;
    std::function<void(const QByteArray &)> changed;
    std::function<void(bool)> picking;
  private:
    void editArea(int index);
    QComboBox *existing_, *type_;
    QSpinBox *bounds_[4], *count_;
    QWidget *area_, *threshold_;
    QPushButton *pick_;
    QLabel *help_, *scope_;
    QByteArray record_;
    bool loading_ = false;
};
}
