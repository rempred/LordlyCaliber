#pragma once
#include "engine/project.h"
#include "engine/event_flow.h"
#include <QWidget>
#include <functional>
class QListWidget;
class QComboBox;
class QLineEdit;
class QScrollArea;
class QLabel;
class QUndoStack;
class QFormLayout;
namespace lordly {
class EventFlowEditor : public QWidget {
  public:
    EventFlowEditor(Project *,QUndoStack *,std::function<void()>,QWidget *parent=nullptr);
    void setRow(int, int entry = -1);
    void selectOffset(int);
    void editChartStep(int row,int offset,QWidget *owner);
    int selectedOffset() const;
    void editTree(QString id = {},QVector<int> path = {},int offset=-1,QJsonObject draft={});
    std::function<void(int)> rawInstructions;
    std::function<void(int selector,int row,int requestOffset)> editCutscene;
    std::function<void(int)> openEventProgram;
    std::function<void(int,int)> openEventInstruction;
  private:
    void showEvent();
    void filter();
    void addField(QFormLayout *,StoryField);
    void addCondition(QFormLayout *,const StoryCondition &);
    void change(StoryField,int);
    void editWhen();
    void editFlags();
    void showDecisions();
    QString destination(int) const;
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    int row_=-1;
    int page_=0;
    int entryFilter_=-1, revealedEntry_=-1, focusOffset_=-1;
    QString selectedAuthoredId_;
    QVector<StoryEvent> events_;
    QVector<PlayerDecision> decisions_;
    bool originalSceneText_=false;
    QByteArray program_;
    SceneEvents *chartDraft_=nullptr;
    QListWidget *list_;
    QComboBox *triggerFilter_;
    QLineEdit *search_;
    QScrollArea *scroll_;
    QLabel *notice_;
};
}
