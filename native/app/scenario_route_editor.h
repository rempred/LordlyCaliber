#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QComboBox;
class QListWidget;
class QSpinBox;
class QLabel;
class QPushButton;
class QTextBrowser;
class QCheckBox;
namespace lordly {
class SquadOrderEditor;
class PendingEdits;
class ScenarioRouteEditor : public QWidget {
  public:
    ScenarioRouteEditor(Project *project,
                        std::function<void(const Scenarios &, const QString &)> commit,
                        QWidget *parent = nullptr);
    void refresh(int key);
    void selectNode(int row);
    void selectSquad(int row);
    void selectDraft(const QString &kind, int row);
    std::function<void(int)> squadSelected;

  private:
    void showNode();
    void showSquadRoute();
    void showCommand();
    void showCondition();
    void showGroup();
    void nodeControls();
    void conditionLabels();
    QString draftKey(const QString &kind, int row) const;
    void rememberDraft(QWidget *, const QString &, int, const QByteArray &, QPushButton *);
    void restoreDraft(QWidget *, const QString &, int, const QByteArray &, QPushButton *);
    struct Draft { QByteArray source; QVariantMap values; };
    QMap<QString,Draft> drafts_;
    QMap<QString,QVariantMap> baselines_;
    PendingEdits *pending_;
    bool applyDrafts();
    void checkDraft(const QString &kind, int row, const QByteArray &source) const;
    void restoreStart();
    QPushButton *discardNode_, *discardCondition_, *discardGroup_;
    QWidget *groupDetails_;
    void applyNode();
    void applyCondition();
    void change(const QString &label, const std::function<void(Scenarios &)> &edit);
    Project *project_;
    std::function<void(const Scenarios &, const QString &)> commit_;
    QListWidget *nodes_, *conditions_;
    QComboBox *routeSquad_, *routeStart_;
    QTextBrowser *routeSummary_;
    SquadOrderEditor *orders_;
    QWidget *advanced_;
    QCheckBox *advancedToggle_;
    QComboBox *nodeKind_, *move_, *wait_, *placement_, *conditionA_, *gate_, *conditionB_, *next_,
        *fork_, *conditionKind_, *groupSquad_, *pattern_;
    QSpinBox *x_, *z_, *group_;
    QVector<QSpinBox *> payload_;
    QVector<QLabel *> payloadLabels_;
    QLabel *extension_, *notice_, *conditionNote_, *nodeImpact_;
    QPushButton *deleteCondition_;
    QWidget *nodeDetails_, *conditionDetails_;
    int key_ = -1, nodeRow_ = 0, conditionRow_ = 0;
    bool loading_ = true;
};
} // namespace lordly
