#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QComboBox;
class QListWidget;
class QSpinBox;
class QLabel;
class QPushButton;
namespace lordly {
class ScenarioRouteEditor : public QWidget {
  public:
    ScenarioRouteEditor(Project *project,
                        std::function<void(const Scenarios &, const QString &)> commit,
                        QWidget *parent = nullptr);
    void refresh(int key);
    void selectNode(int row);

  private:
    void showNode();
    void showCondition();
    void showGroup();
    void nodeControls();
    void conditionLabels();
    void applyNode();
    void applyCondition();
    void change(const QString &label, const std::function<void(Scenarios &)> &edit);
    Project *project_;
    std::function<void(const Scenarios &, const QString &)> commit_;
    QListWidget *nodes_, *conditions_;
    QComboBox *nodeKind_, *move_, *wait_, *placement_, *conditionA_, *gate_, *conditionB_, *next_,
        *fork_, *conditionKind_, *groupSquad_, *pattern_;
    QSpinBox *x_, *z_, *group_;
    QVector<QSpinBox *> payload_;
    QVector<QLabel *> payloadLabels_;
    QLabel *extension_, *notice_, *conditionNote_;
    QPushButton *deleteCondition_;
    QWidget *nodeDetails_, *conditionDetails_;
    int key_ = -1, nodeRow_ = 0, conditionRow_ = 0;
    bool loading_ = true;
};
} // namespace lordly
