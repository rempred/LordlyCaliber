#include "app/scene_timeline.h"
#include <QGraphicsItem>
#include <QMouseEvent>
#include <QScrollBar>
#include <algorithm>
namespace lordly {
namespace { constexpr int gutter=140; constexpr double pixelsPerTick=4.; }
SceneTimeline::SceneTimeline(QWidget *parent):QGraphicsView(parent){
    setObjectName("sceneTimeline");setScene(new QGraphicsScene(this));setMinimumHeight(160);
    setAlignment(Qt::AlignLeft|Qt::AlignTop);setBackgroundBrush(QColor("#faf3df"));
}
void SceneTimeline::setProgram(SceneId id,const DirectorProgram &p){
    if(id_==id&&program_.source==p.source)return;
    id_=id;program_=p;times_.clear();command_=-1;tick_=0;rebuild();
}
void SceneTimeline::updatePlayback(SceneId id,const QMap<int,int> &times,int word,int tick){
    Q_UNUSED(word);if(id!=id_)return;const bool changed=times_!=times;times_=times;tick_=tick;
    if(changed||!playhead_)rebuild();
    else playhead_->setLine(gutter+tick_*pixelsPerTick,0,gutter+tick_*pixelsPerTick,scene()->sceneRect().height());
}
void SceneTimeline::selectCommand(int command){if(command_==command)return;command_=command;rebuild();}
void SceneTimeline::rebuild(){
    const int horizontal=horizontalScrollBar()->value(),vertical=verticalScrollBar()->value();
    scene()->clear();playhead_=nullptr;
    struct Clip {int command,track,lane;double x,width;QString label,tip;bool pending;};
    QVector<Clip> clips;QStringList tracks{"Scene / camera","Dialogue / choices","Logic / waits"};
    const auto actions=SceneActions::inspect(program_);const auto choices=SceneActions::choices(program_);
    int reached=tick_;for(int t:times_)reached=qMax(reached,t);
    const double pendingStart=gutter+(reached+30)*pixelsPerTick;
    QMap<int,QVector<double>> laneEnds;QMap<int,double> pendingEnds;
    double width=1000;
    for(const auto &a:actions){
        const auto &c=program_.commands[a.command];QString track="Scene / camera";
        if(c.opcode==0xbf)track="Dialogue / choices";
        else if(a.actorOperand>=0){
            const auto slot=SceneActions::actorSlotWord(c,a.actorOperand);
            track=(slot&0xffffff00U)==0x08880000U ? "Actor from launch" : QString("Actor %1").arg(qint32(slot));
        }
        else if(a.name=="Place sprite"||a.name=="Move sprite")track=QString("Sprite %1").arg(c.operands[0]&0xffff);
        else if(c.opcode==0x46||c.opcode==0x63||c.opcode==0x64||c.opcode==0x66)track=QString("Sprite %1").arg(quint8(c.operands[0]));
        else if(a.timing!=SceneActionTiming::None||c.opcode==0x8e||c.opcode==0x8f)track="Logic / waits";
        QString label=a.name;
        for(const auto &choice:choices)if(const int branch=choice.option(a.command);branch){track+=QString(" · option %1").arg(branch);label=QString("Option %1 · ").arg(branch)+label;}
        if(!tracks.contains(track))tracks<<track;const int r=tracks.indexOf(track);
        const int observed=times_.value(c.word,-1);int duration=0;
        if(c.opcode==7)duration=int(c.operands[5]);
        else if(a.name=="Move sprite")duration=int(program_.commands[a.command+2].operands[1]);
        else if(a.timing==SceneActionTiming::Counter||a.timing==SceneActionTiming::SharedCounter){const auto &f=a.fields.first();duration=int(f.text(program_.commands[a.command+f.commandOffset].operands[f.operand]).toLongLong());}
        const double x=observed<0?qMax(pendingStart,pendingEnds.value(r)):gutter+observed*pixelsPerTick;
        const double w=qBound(120.,duration*pixelsPerTick,1200.);
        auto &ends=laneEnds[r];int lane=0;while(lane<ends.size()&&ends[lane]>x)++lane;
        if(lane==ends.size())ends<<0.;ends[lane]=x+w+5;
        if(observed<0)pendingEnds[r]=x+w+5;
        const QString tip=label+(observed<0?"\nNot reached in this preview. Its time depends on earlier waits and choices.":QString("\nStart: update %1").arg(observed));
        if(observed<0)label+=" · pending";
        clips<<Clip{a.command,r,lane,x,w,label,tip,observed<0};width=qMax(width,x+w+20);
    }
    QVector<int> order;for(int r=0;r<tracks.size();++r)order<<r;
    std::stable_sort(order.begin(),order.end(),[&](int a,int b){return (tracks[a].startsWith("Actor")||tracks[a].startsWith("Sprite"))>(tracks[b].startsWith("Actor")||tracks[b].startsWith("Sprite"));});
    QVector<int> tops(tracks.size()),bottoms(tracks.size());int height=30;
    for(int r:order){tops[r]=height;height+=qMax(1,int(laneEnds.value(r).size()))*34+15;bottoms[r]=height;}
    for(int t=0;t<=qMax(150,reached+30);t+=30){const auto x=gutter+t*pixelsPerTick;
        scene()->addLine(x,25,x,height,QPen(QColor("#ded1ac")));
        auto label=scene()->addSimpleText(QString::number(t)+" updates");label->setPos(x+3,3);
    }
    for(int r=0;r<tracks.size();++r){
        auto name=scene()->addSimpleText(fontMetrics().elidedText(tracks[r],Qt::ElideRight,gutter-10));name->setPos(5,tops[r]+6);name->setToolTip(tracks[r]);
        scene()->addLine(0,bottoms[r],width,bottoms[r],QPen(QColor("#c6ab70")));
    }
    for(const auto &c:clips){const auto y=tops[c.track]+c.lane*34+2;
        auto box=scene()->addRect(c.x,y,c.width,30,QPen(QColor("#b49356")),QBrush(c.command==command_?QColor("#882c22"):c.pending?QColor("#eee2bf"):QColor("#ecd49b")));
        box->setData(0,c.command);box->setToolTip(c.tip);
        auto text=scene()->addSimpleText(fontMetrics().elidedText(c.label,Qt::ElideRight,int(c.width)-10));text->setParentItem(box);text->setPos(c.x+5,y+3);
        text->setBrush(c.command==command_?Qt::white:QColor("#442b15"));text->setData(0,c.command);text->setToolTip(c.tip);
    }
    playhead_=scene()->addLine(gutter+tick_*pixelsPerTick,0,gutter+tick_*pixelsPerTick,height,QPen(QColor("#ac251c"),2));
    scene()->setSceneRect(0,0,width,height+10);horizontalScrollBar()->setValue(horizontal);verticalScrollBar()->setValue(vertical);
}
void SceneTimeline::mousePressEvent(QMouseEvent *e){
    if(e->button()==Qt::LeftButton){const auto item=itemAt(e->position().toPoint());
        if(item&&item->data(0).isValid()){emit commandSelected(item->data(0).toInt());return;}
        const auto p=mapToScene(e->position().toPoint());if(p.x()>=gutter){emit timeSelected(qMax(0,qRound((p.x()-gutter)/pixelsPerTick)));return;}
    }QGraphicsView::mousePressEvent(e);
}
void SceneTimeline::mouseDoubleClickEvent(QMouseEvent *e){
    const auto item=itemAt(e->position().toPoint());if(item&&item->data(0).isValid()){emit commandActivated(item->data(0).toInt());return;}
    QGraphicsView::mouseDoubleClickEvent(e);
}
}
