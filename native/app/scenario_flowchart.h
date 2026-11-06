#pragma once
#include "app/scenario_flowchart_model.h"
#include "app/event_tree_dialog.h"
#include <QWidget>
#include <functional>
class QGraphicsView;
class QGraphicsScene;
class QLabel;
class QListWidget;
class QPushButton;
namespace lordly {
class ScenarioFlowchart final : public QWidget {
  public:
    explicit ScenarioFlowchart(Project *,QWidget *parent=nullptr);
    void setScenario(QString key,QVector<ChartEvent>);
    void focusTree(int row,const QString &tree,const QVector<int> &path);
    std::function<void(ChartAddress)> edit;
    std::function<void(ChartAddress,ChartTreeEdit)> editStructure;
    std::function<void(int)> createEvent;
    std::function<void(ChartAddress)> navigate;
    std::function<void(ChartAddress,int group,bool remove)> editGate;
  private:
    using Index=QPair<int,int>;
    void draw();
    void drawGroup(const QVector<int> &,qreal &y);
    void drawEvent(int event,qreal y,qreal &bottom);
    void selectFlag(int event,int step,int flag);
    void findCampaignUses();
    void reveal(int event,int step);
    void activate(QString type,int event,int step,int flag,bool twice);
    void center(Index);
    void updateTools();
    Project *project_;
    QGraphicsView *view_;
    QGraphicsScene *scene_;
    QWidget *references_;
    QLabel *flagName_;
    QListWidget *uses_;
    QPushButton *campaign_;
    QPushButton *create_,*insert_,*remove_;
    Index selected_{-1,-1};
    QVector<ChartAddress> useAddresses_;
    QVector<ChartEvent> events_;
    QSet<int> expanded_;
    QMap<Index,QRectF> positions_;
    QString key_;
    int flag_=-1,scope_=-1;
    Index source_{-1,-1};
};
}
