#include "app/scenario_flowchart.h"
#include "engine/event_flag_links.h"
#include "engine/event_authoring.h"
#include "engine/project.h"
#include <QApplication>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QTextDocument>
#include <QVBoxLayout>
#include <algorithm>

namespace lordly {
namespace {
constexpr qreal cardWidth=250, colGap=70;
const QColor ink("#38291e"), burgundy("#812e23"), gold("#ad8b52");
class Canvas : public QGraphicsView {
  public:
    std::function<void(QString,int,int,int,bool)> pick;
    QPoint pressed;
    bool doubled=false;
    void dispatch(const QPoint &p,bool twice){
        auto item=itemAt(p);
        while(item&&!item->data(0).isValid())item=item->parentItem();
        if(item&&pick)pick(item->data(0).toString(),item->data(1).toInt(),item->data(2).toInt(),item->data(3).toInt(),twice);
    }
    void mousePressEvent(QMouseEvent *e) override {pressed=e->position().toPoint();doubled=false;QGraphicsView::mousePressEvent(e);}
    void mouseDoubleClickEvent(QMouseEvent *e) override {
        doubled=true;QGraphicsView::mouseDoubleClickEvent(e);if(e->button()==Qt::LeftButton)dispatch(e->position().toPoint(),true);
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        QGraphicsView::mouseReleaseEvent(e);auto pos=e->position().toPoint();
        if(!doubled&&e->button()==Qt::LeftButton&&(pressed-pos).manhattanLength()<QApplication::startDragDistance())dispatch(pos,false);
    }
};
void tag(QGraphicsItem *item,QString type,int e,int s=-1,int flag=-1){
    item->setData(0,type);item->setData(1,e);item->setData(2,s);item->setData(3,flag);
    item->setCursor(Qt::PointingHandCursor);
}
QGraphicsTextItem *textItem(QGraphicsScene *scene,const QString &text,qreal wrap,bool bold,QColor color) {
    auto item=scene->addText(text);
    auto font=QApplication::font();font.setBold(bold);item->setFont(font);
    item->setDefaultTextColor(color);item->document()->setDocumentMargin(0);
    if(wrap>0)item->setTextWidth(wrap);
    return item;
}
QGraphicsPathItem *plate(QGraphicsScene *scene,QRectF bounds,QColor fill,QColor stroke){
    QPainterPath p;p.addRoundedRect(bounds,5,5);auto card=scene->addPath(p,QPen(stroke),fill);
    return card;
}
void line(QGraphicsScene *scene,QVector<QPointF> points,QColor color,bool dotted=false){
    QPainterPath path(points.first());for(int i=1;i<points.size();++i)path.lineTo(points[i]);
    auto pen=QPen(color,dotted?1.5:1.3,dotted?Qt::DashLine:Qt::SolidLine);scene->addPath(path,pen)->setZValue(-2);
    if(dotted)return;
    const QPointF end=points.last();QLineF shaft(points[points.size()-2],end);
    if(shaft.length()<1)return;shaft.setLength(7);
    const auto d=shaft.p2()-shaft.p1(),n=QPointF(-d.y(),d.x())*.55;
    scene->addPolygon(QPolygonF{end,end-d+n,end-d-n},Qt::NoPen,color)->setZValue(-1);
}
}
ScenarioFlowchart::ScenarioFlowchart(Project *project,QWidget *parent):QWidget(parent),project_(project){
    auto outer=new QVBoxLayout(this);outer->setContentsMargins(0,0,0,0);
    auto tools=new QHBoxLayout;outer->addLayout(tools);
    auto button=[&](QString name,auto callback){auto b=new QPushButton(name);tools->addWidget(b);connect(b,&QPushButton::clicked,this,callback);return b;};
    create_=button("Create event…",[this]{
        if(createEvent)createEvent(selected_.first>=0?events_[selected_.first].address.row:-1);
    });
    insert_=new QPushButton("Add step");tools->addWidget(insert_);
    auto menu=new QMenu(insert_);insert_->setMenu(menu);
    connect(menu,&QMenu::aboutToShow,this,[this,menu]{
        menu->clear();const auto [e,s]=selected_;if(e<0)return;
        const auto &event=events_[e];const auto address=s<0?event.address:event.steps[s].address;
        if(address.tree.isEmpty()){
            if(address.gate>=0)menu->addAction("IF / ELSE before this condition…",this,[this,address,e]{if(editGate)editGate(address,events_[e].group,false);});
            return;
        }
        auto choices=[this](QMenu *list,ChartAddress target){
            for(const auto &[name,op]:{qMakePair(QString("IF / ELSE"),ChartTreeEdit::AddDecision),qMakePair(QString("Action…"),ChartTreeEdit::AddAction)})
                list->addAction(name,this,[this,target,op]{if(editStructure)editStructure(target,op);});
        };
        if(s>=0&&event.steps[s].decision){
            // A leaf of an AND/OR group still belongs to the same IF.
            int i=0;while(i+1<address.path.size()&&address.path[i+1]!=0)i+=2;
            auto branch=address;branch.path=address.path.mid(0,i+1);branch.path.append(1);
            choices(menu->addMenu("On Yes"),branch);branch.path.last()=2;choices(menu->addMenu("On No"),branch);
        }else choices(menu,address);
    });
    remove_=button("Delete step…",[this]{const auto [e,s]=selected_;if(e<0)return;
        const auto &address=s<0?events_[e].address:events_[e].steps[s].address;
        if(address.tree.isEmpty()){if(editGate)editGate(address,events_[e].group,true);}
        else if(editStructure)editStructure(address,ChartTreeEdit::Remove);
    });
    button("Expand all",[this]{for(int i=0;i<events_.size();++i)expanded_.insert(i);draw();});
    button("Collapse all",[this]{expanded_.clear();draw();});
    tools->addStretch();
    button("Zoom out",[this]{if(view_->transform().m11()>.4)view_->scale(1/1.2,1/1.2);});
    button("Zoom in",[this]{if(view_->transform().m11()<2.5)view_->scale(1.2,1.2);});
    auto split=new QSplitter;outer->addWidget(split,1);
    auto canvas=new Canvas;view_=canvas;view_->setObjectName("scenarioFlowCanvas");scene_=new QGraphicsScene(this);
    view_->setScene(scene_);view_->setAlignment(Qt::AlignTop|Qt::AlignLeft);view_->setBackgroundBrush(QColor("#faf3e3"));
    view_->setRenderHint(QPainter::Antialiasing);view_->setDragMode(QGraphicsView::ScrollHandDrag);split->addWidget(view_);
    canvas->pick=[this](auto... args){activate(args...);};
    references_=new QWidget;auto side=new QVBoxLayout(references_);
    auto heading=new QHBoxLayout;flagName_=new QLabel;flagName_->setWordWrap(true);heading->addWidget(flagName_,1);
    auto close=new QPushButton("Close");heading->addWidget(close);side->addLayout(heading);
    uses_=new QListWidget;uses_->setObjectName("scenarioFlowFlagUses");uses_->setWordWrap(true);uses_->setTextElideMode(Qt::ElideNone);side->addWidget(uses_);
    campaign_=new QPushButton("Find all campaign uses");side->addWidget(campaign_);
    connect(campaign_,&QPushButton::clicked,this,[this]{findCampaignUses();});
    connect(close,&QPushButton::clicked,this,[this]{flag_=-1;references_->hide();draw();});
    connect(uses_,&QListWidget::itemClicked,this,[this](auto item){
        if(item->data(Qt::UserRole).isValid())reveal(item->data(Qt::UserRole).toInt(),item->data(Qt::UserRole+1).toInt());
    });
    connect(uses_,&QListWidget::itemActivated,this,[this](auto item){
        const int i=uses_->row(item);if(i>=0&&i<useAddresses_.size()&&navigate)navigate(useAddresses_[i]);
    });
    split->addWidget(references_);split->setSizes({920,300});references_->hide();
}
void ScenarioFlowchart::setScenario(QString key,QVector<ChartEvent> events){
    const bool same=key_==key;
    // A project edit can insert a handler ahead of the open ones. Anchor by
    // authored identity or original entry, never by yesterday's list position.
    QSet<QPair<int,QString>> keep;
    const auto identity=[](const ChartEvent &e){return qMakePair(e.address.row,e.address.tree.isEmpty()?QString::number(e.address.offset):e.address.tree);};
    if(same)for(int i:expanded_)keep.insert(identity(events_[i]));
    key_=key;events_=std::move(events);expanded_.clear();
    for(int i=0;i<events_.size();++i)if(keep.contains(identity(events_[i])))expanded_.insert(i);
    flag_=-1;selected_={-1,-1};references_->hide();updateTools();draw();
    if(!same){view_->resetTransform();view_->horizontalScrollBar()->setValue(0);view_->verticalScrollBar()->setValue(0);}
}
void ScenarioFlowchart::draw(){
    const auto corner=view_->mapToScene(QPoint(0,0));
    scene_->clear();positions_.clear();
    QStringList groups;QMap<QString,QVector<int>> buckets;
    for(int i=0;i<events_.size();++i){
        const auto group=events_[i].section+"  /  "+events_[i].trigger;
        if(!buckets.contains(group))groups<<group;buckets[group]<<i;
    }
    qreal y=12;
    for(const auto &group:groups){
        auto label=textItem(scene_,group,-1,true,burgundy);label->setPos(12,y);y+=label->boundingRect().height()+9;
        drawGroup(buckets[group],y);y+=12;
    }
    if(events_.isEmpty())textItem(scene_,"No events are assigned here.",-1,false,ink)->setPos(12,y);
    if(flag_>=0 && positions_.contains(source_)){
        const auto start=positions_[source_].topRight();
        bool sourceReads=false,sourceWrites=false;
        for(const auto &flag:events_[source_.first].steps[source_.second].flags)if(flag.id==flag_){
            if(flag.use=="Checks")sourceReads=true;else sourceWrites=true;
        }
        for(auto it=positions_.cbegin();it!=positions_.cend();++it){
            auto [e,s]=it.key();if(s<0||it.key()==source_)continue;
            if(EventFlagLinks::programScoped(flag_)&&events_[e].address.row!=scope_)continue;
            for(const auto &flag:events_[e].steps[s].flags)if(flag.id==flag_&&(flag.use=="Checks"?sourceWrites:sourceReads)){
                auto end=it.value().topRight();qreal x=qMax(start.x(),end.x())+22;
                line(scene_,{start,{x,start.y()},{x,end.y()},end},burgundy,true);break;
            }
        }
    }
    scene_->setSceneRect(scene_->itemsBoundingRect().adjusted(-10,-10,25,25));
    view_->centerOn(corner+QPointF(view_->viewport()->width(),view_->viewport()->height())/(2*view_->transform().m11()));
}
void ScenarioFlowchart::drawGroup(const QVector<int> &list,qreal &y){
    int col=0;qreal rowHeight=0;
    for(int e:list){
        bool open=expanded_.contains(e);
        if(open&&col){y+=rowHeight+8;col=0;rowHeight=0;}
        const qreal w=open?2*cardWidth+16:cardWidth;
        auto title=textItem(scene_,(open?QString("−  "):QString("+  "))+events_[e].title,w-22,true,open?Qt::white:ink);
        const qreal h=qMax<qreal>(40,title->boundingRect().height()+16);
        QRectF box(12+col*(cardWidth+16),y,w,h);
        auto card=plate(scene_,box,open?burgundy:QColor("#eddbaf"),open?burgundy:gold);tag(card,"event",e);
        title->setParentItem(card);title->setPos(box.topLeft()+QPointF(10,8));positions_[{e,-1}]=box;
        if(open){
            y+=h+30;qreal bottom=y;drawEvent(e,y,bottom);y=bottom+24;col=0;rowHeight=0;
        }else{
            rowHeight=qMax(rowHeight,h);if(++col==3){y+=rowHeight+8;col=0;rowHeight=0;}
        }
    }
    if(col)y+=rowHeight+8;
}
void ScenarioFlowchart::drawEvent(int e,qreal top,qreal &bottom){
    const auto &graph=events_[e];const int count=graph.steps.size();if(!count)return;
    QVector<int> ranks(count),columns(count);
    QVector<QVector<int>> outgoing(count);
    for(int i=0;i<graph.edges.size();++i)outgoing[graph.edges[i].from]<<i;
    for(int s=0;s<count;++s)for(int edge:outgoing[s]){
        int dest=graph.edges[edge].to;if(dest>s)ranks[dest]=qMax(ranks[dest],ranks[s]+1);
    }
    QMap<int,int> nextColumn;QVector<qreal> heights(count),levels(count);
    QVector<QGraphicsPathItem *> cards;
    for(int s=0;s<count;++s){
        const auto &step=graph.steps[s];columns[s]=nextColumn[ranks[s]]++;
        auto title=textItem(scene_,(step.decision?"IF   ":"")+step.title,cardWidth-22,step.decision,ink);
        qreal h=title->boundingRect().height()+16;
        QVector<QGraphicsTextItem*> flags;
        for(const auto &f:step.flags){
            auto flag=textItem(scene_,QString("%1 · Flag %2").arg(f.use).arg(f.id),cardWidth-22,false,burgundy);
            auto font=flag->font();font.setUnderline(true);flag->setFont(font);flags<<flag;
            h+=flag->boundingRect().height()+5;
        }
        const bool lit=flag_>=0&&(!EventFlagLinks::programScoped(flag_)||graph.address.row==scope_)&&
            std::any_of(step.flags.cbegin(),step.flags.cend(),[&](const auto &f){return f.id==flag_;});
        const bool selected=selected_==Index{e,s};
        const QRectF box(0,0,cardWidth,h);auto card=plate(scene_,box,lit||selected?QColor("#f7d89c"):step.decision?QColor("#f6eacb"):QColor("#fffaf0"),lit||selected?burgundy:gold);
        tag(card,"step",e,s);if(step.address.row<0)card->unsetCursor();
        card->setToolTip((step.address.row<0?QString{}:QString("Double-click to edit this step\n"))+step.detail);
        title->setParentItem(card);title->setPos(10,7);
        auto y=title->boundingRect().height()+11;
        for(int i=0;i<flags.size();++i){
            auto flag=flags[i];flag->setParentItem(card);flag->setPos(10,y);tag(flag,"flag",e,s,step.flags[i].id);
            flag->setToolTip(project_->sceneEvents()->flagName(step.flags[i].id,graph.address.row));y+=flag->boundingRect().height()+5;
        }
        cards<<card;heights[ranks[s]]=qMax(heights[ranks[s]],h);
    }
    levels[0]=top;
    for(int i=1;i<count;++i)levels[i]=levels[i-1]+heights[i-1]+65;
    for(int s=0;s<count;++s){
        auto card=cards[s];card->setPos(40+columns[s]*(cardWidth+colGap),levels[ranks[s]]);
        positions_[{e,s}]=card->sceneBoundingRect();bottom=qMax(bottom,card->sceneBoundingRect().bottom());
    }
    const auto root=positions_[{e,0}];
    line(scene_,{{root.center().x(),positions_[{e,-1}].bottom()},{root.center().x(),root.top()}},gold);
    for(int index=0;index<graph.edges.size();++index){
        const auto &edge=graph.edges[index];
        const auto a=positions_[{e,edge.from}],b=positions_[{e,edge.to}];
        qreal port=0;
        if(outgoing[edge.from].size()==2){
            const int first=outgoing[edge.from].first(),last=outgoing[edge.from].last();
            const bool firstLeft=columns[graph.edges[first].to]<=columns[graph.edges[last].to];
            port=((index==first)==firstLeft)?-54:54;
        }
        const qreal x=a.center().x()+port;
        const QPointF start{x,a.bottom()},end{b.center().x(),b.top()};
        const bool adjacent=ranks[edge.to]==ranks[edge.from]+1;
        const qreal mid=start.y()+(port>0?37:23);
        if(adjacent)line(scene_,{start,{x,mid},{end.x(),mid},end},ink);
        else {
            const qreal lane=port>0?qMax(a.right(),b.right())+14+5*(edge.from%4):qMin(a.left(),b.left())-14-5*(edge.from%4);
            line(scene_,{start,{x,mid},{lane,mid},{lane,end.y()-10},{end.x(),end.y()-10},end},ink);
        }
        QString labelText=edge.label;
        if(edge.to<=edge.from)labelText=labelText.isEmpty()?"Back":labelText+" · back";
        if(!labelText.isEmpty()){
            auto label=textItem(scene_,labelText,-1,false,burgundy);label->setPos(x+4,start.y()+1);
        }
    }
}
void ScenarioFlowchart::center(Index i){
    if(positions_.contains(i))view_->ensureVisible(positions_[i],20,24);
}
void ScenarioFlowchart::reveal(int e,int step){
    if(!expanded_.contains(e)){expanded_.insert(e);draw();}center({e,step});
}
void ScenarioFlowchart::focusTree(int row,const QString &tree,const QVector<int> &path){
    for(int e=0;e<events_.size();++e){
        if(events_[e].address.row!=row||events_[e].address.tree!=tree)continue;
        int step=-1;for(int s=0;!path.isEmpty()&&s<events_[e].steps.size();++s)
            if(events_[e].steps[s].address.path==path){step=s;break;}
        expanded_.insert(e);selected_={e,step};updateTools();draw();center(selected_);return;
    }
}
void ScenarioFlowchart::activate(QString type,int e,int s,int flag,bool twice){
    if(e<0||e>=events_.size())return;
    selected_={e,s};updateTools();
    if(type=="event"&&!twice){
        if(expanded_.contains(e))expanded_.remove(e);else expanded_.insert(e);
        draw();center({e,-1});
    }else if(type=="event"&&twice&&edit&&events_[e].address.row>=0)edit(events_[e].address);
    else if(type=="flag"&&!twice)selectFlag(e,s,flag);
    else if(type=="step"&&!twice){draw();center({e,s});}
    else if(type=="step"&&twice&&edit&&events_[e].steps[s].address.row>=0)edit(events_[e].steps[s].address);
}
void ScenarioFlowchart::updateTools(){
    const int e=selected_.first,s=selected_.second;
    const bool authored=e>=0&&!events_[e].address.tree.isEmpty();
    const bool original=e>=0&&s>=0&&events_[e].steps[s].address.gate>=0;
    insert_->setEnabled(original||(authored&&(s<0||events_[e].steps[s].address.row>=0)));
    remove_->setEnabled(original||(authored&&(s<0||!events_[e].steps[s].address.path.isEmpty())));
    remove_->setText(s<0?"Delete event…":"Delete step…");
    create_->setEnabled(std::any_of(events_.cbegin(),events_.cend(),[](const auto &event){return EventAuthoring::supported(event.address.row);}));
}
void ScenarioFlowchart::selectFlag(int event,int step,int id){
    flag_=id;scope_=events_[event].address.row;source_={event,step};
    flagName_->setText(project_->sceneEvents()->flagName(id,scope_));
    uses_->clear();useAddresses_.clear();
    for(int e=0;e<events_.size();++e){
        const auto &g=events_[e];if(EventFlagLinks::programScoped(id)&&g.address.row!=scope_)continue;
        for(int s=0;s<g.steps.size();++s)for(const auto &flag:g.steps[s].flags)if(flag.id==id){
            auto item=new QListWidgetItem(flag.use+" · "+g.title+"\n"+g.steps[s].title,uses_);
            item->setData(Qt::UserRole,e);item->setData(Qt::UserRole+1,s);useAddresses_<<g.steps[s].address;
        }
    }
    campaign_->setEnabled(true);references_->show();draw();center(source_);
}
void ScenarioFlowchart::findCampaignUses(){
    try{
        const auto refs=EventFlagLinks::references(project_->rom(),*project_->sceneEvents(),*project_->scenePrograms(),scope_,flag_);
        // Keep directly editable authored paths, and add hits from programs or
        // shared instructions outside the visible graph.
        for(const auto &ref:refs){
            if(std::any_of(events_.cbegin(),events_.cend(),[&](const auto &event){
                if(event.address.row!=ref.row||ref.programReset)return false;
                if(!ref.authoredId.isEmpty())return event.address.tree==ref.authoredId;
                return std::any_of(event.steps.cbegin(),event.steps.cend(),[&](const auto &s){
                    return (s.address.offset==ref.offset||s.instructions.contains(ref.offset))&&std::any_of(s.flags.cbegin(),s.flags.cend(),[&](const auto &f){return f.id==flag_;});
                });
            }))continue;
            QString use=ref.read?"Checks":ref.programReset?"Clears on program change":ref.state<0?"Sets or clears":ref.state?"Sets ON":"Clears OFF";
            uses_->addItem(use+" · "+ref.title+"\n"+EventContexts::programName(ref.row));
            useAddresses_.append({ref.row,ref.offset,ref.authoredId});
        }
        campaign_->setEnabled(false);
    }catch(const std::exception &error){flagName_->setText(QString::fromUtf8(error.what()));}
}
}
