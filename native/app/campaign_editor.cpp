#include "app/campaign_editor.h"
#include "app/campaign_art_editor.h"
#include "app/text_workspace.h"
#include "app/content_refresh.h"
#include "app/cutscene_editor.h"
#include "app/scenario_editor.h"
#include "app/scenario_event_editor.h"
#include "app/event_flag_links.h"
#include "engine/event_script.h"
#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QImageWriter>
#include <QSaveFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QHideEvent>
#include <QJsonArray>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QShowEvent>
#include <QSpinBox>
#include <QSplitter>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTimer>
#include <QUndoStack>
#include <QVBoxLayout>
#include <algorithm>
namespace lordly {
namespace {
class CampaignCommand final : public QUndoCommand {
  public:
    CampaignCommand(Project *p, Project next, const QString &label)
        : QUndoCommand(label), p_(p), before_(*p), after_(std::move(next)) {}
    void undo() override {
        *p_ = before_;
    }
    void redo() override {
        *p_ = after_;
    }

  private:
    Project *p_;
    Project before_, after_;
};
class MapCommand final : public WorldMapUndoCommand {
  public:
    MapCommand(Project *p,const CampaignMap &map,const QString &label)
        :WorldMapUndoCommand(label),p_(p),before_(p->campaignMap()->json()),after_(map.json()){}
    void undo()override{p_->setCampaignMap(before_);}
    void redo()override{p_->setCampaignMap(after_);}
  private:
    Project *p_;
    QJsonObject before_,after_;
};
QPushButton *button(QBoxLayout *layout, const QString &text, const QString &name) {
    auto b = new QPushButton(text);
    b->setObjectName(name);
    layout->addWidget(b);
    return b;
}
QString conditions(const QJsonArray &a) {
    QStringList text;
    for (const auto &v : a) {
        const auto c = v.toObject();
        text << EventScript::flagName(c["flag"].toInt()) +
                    (c["value"].toBool() ? " = ON" : " = OFF");
    }
    return text.isEmpty() ? QString("Always") : text.join("; ");
}
QString entryIdentity(const EntryConditions &entry) {
    QSet<int> checks;for(const auto &condition:entry.flags)for(int at:condition.comparisonOffsets)checks.insert(at);
    for(const auto &condition:entry.numbers)checks.insert(condition.comparisonOffset);
    auto offsets=checks.values();std::sort(offsets.begin(),offsets.end());QString key=QString::number(entry.eventOffset);
    for(int at:offsets)key+="/"+QString::number(at);return key;
}
} // namespace
class CampaignCanvas final : public QWidget {
  public:
    explicit CampaignCanvas(Project *p) : p_(p) {
        setObjectName("campaignMapCanvas");
        setMinimumSize(480, 300);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_OpaquePaintEvent);
        auto image = [](const RasterImage &r) {
            return QImage(reinterpret_cast<const uchar *>(r.rgba.constData()), r.width, r.height,
                          QImage::Format_RGBA8888)
                .copy();
        };
        background_ = image(p_->campaignMap()->background());
        dot_ = image(p_->campaignMap()->roadArtwork()).copy(0, 0, 6, 6);
        for (int id = 1; id <= 40; ++id)
            {markers_[id] = image(p_->campaignMap()->nodeArtwork(id));markerTypes_[id]=p_->campaignMap()->icon(id);}
        syncMap();
    }
    int selected = 1, selectedRoad = 0;
    std::function<void(int)> picked;
    std::function<void(int, QPoint)> moved;
    std::function<void(int)> roadPicked;
    std::function<void(int,const QVector<QPoint> &)> roadShaped;
    std::function<void(const QString &)> roadMessage;
    std::function<void(int,int)> connectionPicked;
    std::function<void(bool)> connectionModeChanged;
    std::function<void(int,QPoint)> locationPlaced;
    std::function<void(bool)> placementModeChanged;
    bool placingLocation()const{return placementId_>0;}
    void beginPlacement(int id) {
        cancelConnection();placementId_=id;placementCursor_=mapRect().center();
        setCursor(Qt::CrossCursor);setFocus(Qt::OtherFocusReason);
        if(placementModeChanged)placementModeChanged(true);update();
    }
    void cancelPlacement() {
        if(!placingLocation())return;
        placementId_=0;unsetCursor();if(placementModeChanged)placementModeChanged(false);update();
    }
    void syncMap() {
        const auto &map=*p_->campaignMap();
        if(backgroundBytes_!=map.backgroundBytes()) {
            backgroundBytes_=map.backgroundBytes();
            background_=CampaignMap::artworkImage(backgroundBytes_,{512,320},256);cacheSize_={};
        }
        nodePositions_.resize(CampaignMap::nodeCount+1);
        for(int id=1;id<nodePositions_.size();++id) {
            nodePositions_[id]=map.position(id);
            const auto pixels=map.iconFrameBytes(map.icon(id),1,0);
            if(markerTypes_.value(id,-1)==map.icon(id) && markerBytes_.value(id)==pixels)continue;
            markerBytes_[id]=pixels;
            markerTypes_[id]=map.icon(id);
            const auto art=map.nodeArtwork(id);
            const auto image=QImage(reinterpret_cast<const uchar *>(art.rgba.constData()),art.width,art.height,QImage::Format_RGBA8888).copy();
            if(markers_.value(id)!=image){markers_[id]=image;cacheSize_={};}
        }
        roadPoints_.resize(map.totalRoads()+1);
        for(int id=1;id<roadPoints_.size();++id)roadPoints_[id]=map.roadDots(id);
        visibleRoadPoints_=CampaignMap::mergedRoadDots(roadPoints_);
        update();
    }

    bool choosingConnection() const { return connectionSource_ > 0; }
    void beginConnection(int source) {
        cancelPlacement();
        drag_ = dragDot_ = -1;
        connectionSource_ = source;
        connectionCursor_ = screen(p_->campaignMap()->position(source));
        setCursor(Qt::CrossCursor);
        setFocus(Qt::OtherFocusReason);
        if (connectionModeChanged) connectionModeChanged(true);
        update();
    }
    void cancelConnection() {
        if (!choosingConnection()) return;
        connectionSource_ = 0;
        unsetCursor();
        if (connectionModeChanged) connectionModeChanged(false);
        update();
    }

  protected:
    QRectF mapRect() const {
        const double scale = qMin(width() / 512., height() / 320.);
        return {(width() - 512 * scale) / 2, (height() - 320 * scale) / 2, 512 * scale,
                320 * scale};
    }
    QPointF screen(QPoint p) const {
        const auto r = mapRect();
        return r.topLeft() + QPointF(p.x() * r.width() / 512, p.y() * r.height() / 320);
    }
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(32, 29, 24));
        const auto bounds = mapRect();
        const double scale = bounds.width() / 512.;
        const auto ratio=devicePixelRatioF();
        if(cacheSize_!=size() || cacheRatio_!=ratio) {
            auto scaled=[&](const QImage &image,QSizeF size) {
                auto pixmap=QPixmap::fromImage(image.scaled((size*ratio).toSize(),Qt::IgnoreAspectRatio,Qt::FastTransformation));
                pixmap.setDevicePixelRatio(ratio);return pixmap;
            };
            backgroundCache_=scaled(background_,bounds.size());dotCache_=scaled(dot_,QSizeF(dot_.size())*scale);
            for(auto it=markers_.begin();it!=markers_.end();++it)markerCache_[it.key()]=scaled(it.value(),QSizeF(it->size())*scale);
            cacheSize_=size();cacheRatio_=ratio;
        }
        painter.drawPixmap(bounds.topLeft(),backgroundCache_);
        auto draw = [&](QPoint p, const QPixmap &art) {
            painter.drawPixmap(screen(p),art);
        };
        QSet<int> drawn;
        for (int id = 1; id < visibleRoadPoints_.size(); ++id)
            for (const auto point:visibleRoadPoints_[id]) {
                const int key=point.y()*512+point.x();
                if(!drawn.contains(key)){draw(point,dotCache_);drawn.insert(key);}
            }
        if(selectedRoad>0 && selectedRoad<visibleRoadPoints_.size()) {
            painter.setPen(QPen(QColor(255,225,110),1));painter.setBrush(Qt::NoBrush);
            for(const auto point:visibleRoadPoints_[selectedRoad])
                painter.drawEllipse(screen(point+QPoint(3,3)),5*scale,5*scale);
        }
        if (choosingConnection()) {
            painter.save();
            painter.setClipRect(bounds);
            painter.setPen(QPen(QColor(255,225,110), 2, Qt::DashLine));
            painter.drawLine(screen(nodePositions_[connectionSource_]), connectionCursor_);
            painter.restore();
        }
        for (int id = 1; id <= CampaignMap::nodeCount; ++id) {
            const auto point = nodePositions_[id];
            if (!point.x())
                continue;
            draw(point + QPoint(-4, -8), markerCache_[id]);
            if (id == selected) {
                painter.setPen(QPen(QColor(255, 225, 110), 2));
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(screen(point + QPoint(3, 0)), 11 * scale, 11 * scale);
            }
        }
        if(placingLocation() && bounds.contains(placementCursor_)) {
            painter.setOpacity(0.7);
            painter.drawPixmap(placementCursor_-QPointF(4*scale,8*scale),markerCache_[placementId_]);
        }
    }
    void keyPressEvent(QKeyEvent *e) override {
        if (e->key() == Qt::Key_Escape && (choosingConnection() || placingLocation())) {
            cancelConnection();cancelPlacement();
            e->accept();
            return;
        }
        QWidget::keyPressEvent(e);
    }
    void hideEvent(QHideEvent *e) override {
        cancelConnection();
        cancelPlacement();
        drag_ = dragDot_ = -1;dragMap_.reset();
        QWidget::hideEvent(e);
    }
    void showEvent(QShowEvent *e) override {
        syncMap();
        QWidget::showEvent(e);
    }
    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() == Qt::RightButton && (choosingConnection() || placingLocation())) {
            cancelConnection();cancelPlacement();
            return;
        }
        if (e->button() != Qt::LeftButton)
            return;
        int hit = -1;
        double distance = qMax(14., mapRect().width() / 512. * 10);
        for (int id = 1; id <= CampaignMap::nodeCount; ++id)
            if (p_->campaignMap()->position(id).x()) {
                const auto d =
                    QLineF(screen(p_->campaignMap()->position(id)), e->position()).length();
                if (d < distance) {
                    hit = id;
                    distance = d;
                }
            }
        if (placingLocation()) {
            const auto r=mapRect();
            if(hit<0 && r.contains(e->position()) && locationPlaced) {
                const auto point=e->position()-r.topLeft();
                locationPlaced(placementId_,{qBound(1,qRound(point.x()*512/r.width()),511),qBound(1,qRound(point.y()*320/r.height()),319)});
            }
            return;
        }
        if (choosingConnection()) {
            if (hit > 0 && connectionPicked) connectionPicked(connectionSource_, hit);
            return;
        }
        if (hit < 0) {
            double closest=qMax(7.,mapRect().width()/512.*5);
            int road=0,dot=-1;QPoint point;
            for(int id=1;id<=p_->campaignMap()->totalRoads();++id) {
                const auto &dots=visibleRoadPoints_[id];
                for(int n=0;n<dots.size();++n) {
                    const auto distance=QLineF(screen(dots[n]+QPoint(3,3)),e->position()).length();
                    if(distance<closest || (road && qAbs(distance-closest)<0.01 && id==selectedRoad)){
                        closest=distance;road=id;dot=n;point=roadPoints_[id][n];
                    }
                }
            }
            if(!road)return;
            selectedRoad=road;dragDot_=dot;preview_=originPoint_=point;origin_=e->position();
            roadDragValid_=true;if(roadMessage)roadMessage({});
            if(roadPicked)roadPicked(road);
            dragMap_=*p_->campaignMap();previewBends_=dragMap_->roadWaypoints(road);
            update();return;
        }
        selected = drag_ = hit;
        preview_ = originPoint_ = p_->campaignMap()->position(hit);
        origin_ = e->position();
        if (picked)
            picked(hit);
        dragMap_=*p_->campaignMap();
        update();
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        if(placingLocation()){placementCursor_=e->position();update();return;}
        if (choosingConnection()) {
            connectionCursor_ = e->position();
            update();
            return;
        }
        if (drag_ < 0 && dragDot_ < 0) return;
        const auto r=mapRect();const auto delta=e->position()-origin_;
        preview_={qBound(1,originPoint_.x()+qRound(delta.x()*512/r.width()),511),
                  qBound(1,originPoint_.y()+qRound(delta.y()*320/r.height()),319)};
        auto map=*dragMap_;
        if(dragDot_>=0) {
            try {
                previewBends_=dragMap_->pulledRoad(selectedRoad,dragDot_,preview_-originPoint_);
                map.setRoadDots(selectedRoad,previewBends_);roadPoints_[selectedRoad]=map.roadDots(selectedRoad);
                roadDragValid_=true;if(roadMessage)roadMessage({});
            } catch(const Error &error) {
                roadDragValid_=false;roadPoints_[selectedRoad]=dragMap_->roadDots(selectedRoad);
                if(roadMessage)roadMessage(QString::fromUtf8(error.what()));
            }
        } else {
            map.setNode(drag_,preview_,map.label(drag_));nodePositions_[drag_]=preview_;
            for(int id=1;id<roadPoints_.size();++id){const auto ends=map.road(id);if(ends.first==drag_ || ends.second==drag_)roadPoints_[id]=map.roadDots(id);}
        }
        visibleRoadPoints_=CampaignMap::mergedRoadDots(roadPoints_);
        update();
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        if (dragDot_>=0) {
            dragDot_=-1;dragMap_.reset();
            if (roadDragValid_ && preview_!=originPoint_ && roadShaped) roadShaped(selectedRoad,previewBends_);
            syncMap();return;
        }
        if (drag_ < 0) return;
        const int node = drag_;
        drag_ = -1;
        dragMap_.reset();
        if (QLineF(origin_, e->position()).length() > 3 && moved)
            moved(node, preview_);
        syncMap();
    }

  private:
    Project *p_;
    int drag_ = -1, dragDot_ = -1;
    bool roadDragValid_ = true;
    int connectionSource_ = 0;
    int placementId_ = 0;
    QPointF placementCursor_;
    QPointF connectionCursor_;
    std::optional<CampaignMap> dragMap_;
    QVector<QPoint> nodePositions_,previewBends_;
    QVector<QVector<QPoint>> roadPoints_,visibleRoadPoints_;
    QSize cacheSize_;
    qreal cacheRatio_ = 0;
    QPixmap backgroundCache_,dotCache_;
    QMap<int,QPixmap> markerCache_;
    QPoint preview_, originPoint_;
    QPointF origin_;
    QImage background_, dot_;
    QMap<int, QImage> markers_;
    QMap<int,int> markerTypes_;
    QByteArray backgroundBytes_;
    QMap<int,QByteArray> markerBytes_;
};
CampaignEditor::CampaignEditor(Project *p, QUndoStack *undo, std::function<void()> changed,
                               QWidget *parent)
    : QWidget(parent), project_(p), undo_(undo), changed_(std::move(changed)), routes_(p->rom()) {
    setObjectName("campaignEditor");
    p->loadCampaignMap();
    p->loadTexts();
    p->loadSceneEvents();
    p->loadScenarios();
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    tabs_ = new QTabWidget;
    tabs_->setObjectName("campaignTabs");
    layout->addWidget(tabs_);
    auto world = new QWidget;
    auto worldLayout = new QVBoxLayout(world);
    auto actions = new QHBoxLayout;
    auto create = button(actions, "Create map location", "campaignCreateNode");
    auto artwork = button(actions,"Replace map artwork…","campaignReplaceArtwork");
    auto exportArtwork = button(actions,"Export PNG…","campaignExportArtwork");
    connect(exportArtwork,&QPushButton::clicked,this,[this]{
        try {
            const auto path=QFileDialog::getSaveFileName(this,"Export world map artwork","World map.png","PNG (*.png)");
            if(path.isEmpty())return;
            QSaveFile file(path);
            if(!file.open(QIODevice::WriteOnly))throw Error("Could not create the map PNG: "+file.errorString());
            const auto image=CampaignMap::artworkImage(project_->campaignMap()->backgroundBytes(),{512,320},256);
            QImageWriter writer(&file,"png");
            if(!writer.write(image))throw Error("Could not export map artwork: "+writer.errorString());
            if(!file.commit())throw Error("Could not finish the map PNG: "+file.errorString());
            notice_->setText("Map artwork exported.");
        }catch(const std::exception &e){notice_->setText(QString::fromUtf8(e.what()));}
    });
    connect(artwork,&QPushButton::clicked,this,[this]{
        try{replaceCampaignBackground(this,project_,undo_);}catch(const std::exception &e){notice_->setText(QString::fromUtf8(e.what()));}
    });

    actions->addStretch();
    worldLayout->addLayout(actions);
    auto split = new QSplitter;
    worldLayout->addWidget(split, 1);
    nodes_ = new QListWidget;
    nodes_->setObjectName("campaignNodes");
    nodes_->setAccessibleName("Map locations");
    nodes_->setMinimumWidth(200);
    split->addWidget(nodes_);
    canvas_ = new CampaignCanvas(p);
    split->addWidget(canvas_);
    auto fields = new QWidget;
    auto detail = new QVBoxLayout(fields);
    auto form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    label_ = new QLineEdit;
    label_->setMaxLength(48);
    label_->setObjectName("campaignNodeName");
    x_ = new QSpinBox;
    y_ = new QSpinBox;
    x_->setRange(1, 511);
    y_->setRange(1, 319);
    x_->setObjectName("campaignNodeX");
    y_->setObjectName("campaignNodeY");
    form->addRow("Map location name", label_);
    icon_=new QComboBox;icon_->setObjectName("campaignLocationIcon");icon_->setIconSize({32,32});
    auto iconRow=new QHBoxLayout;iconRow->addWidget(icon_,1);
    auto editIcons=button(iconRow,"Edit icons…","campaignEditIcons");
    form->addRow("Location icon",iconRow);
    connect(editIcons,&QPushButton::clicked,this,[this]{
        try{editCampaignIcons(this,project_,undo_,icon_->currentData().toInt());}catch(const std::exception &e){notice_->setText(QString::fromUtf8(e.what()));}
    });
    form->addRow("X", x_);
    form->addRow("Y", y_);
    detail->addLayout(form);
    auto apply = button(detail, "Apply location", "campaignApplyNode");
    auto branch = button(detail, "Edit storyline…", "campaignEditEntryRules");
    auto roadBox = new QGroupBox("Road connections");
    auto roadLayout = new QVBoxLayout(roadBox);
    roads_ = new QListWidget; roads_->setObjectName("campaignRoads");
    roadLayout->addWidget(roads_);
    auto roadActions=new QHBoxLayout;
    auto addRoad=button(roadActions,"Add connection","campaignAddRoad");
    auto removeRoad=button(roadActions,"Remove connection","campaignRemoveRoad");
    roadLayout->addLayout(roadActions);

    detail->addWidget(roadBox);
    description_ = new DescriptionPanel(p, undo, changed_);
    description_->setObjectName("campaignLocationDescription");
    detail->addWidget(description_);
    detail->addStretch();
    split->addWidget(fields);
    split->setSizes({210, 600, 310});
    tabs_->addTab(world, "World Map");
    scenarios_ = new ScenarioEditor(p, undo, changed_);
    tabs_->addTab(scenarios_, "Scenarios");
    story_ = new QTabWidget;
    story_->setObjectName("storylineTabs");
    auto flowPage = new QWidget;
    auto flowLayout = new QVBoxLayout(flowPage);
    auto flowControls = new QHBoxLayout;
    flowControls->addWidget(new QLabel("Preview entry routes at Chaos Frame"));
    flowChaos_ = new QSpinBox;
    flowChaos_->setRange(0, 100);
    flowChaos_->setValue(50);
    flowChaos_->setObjectName("storylineChaos");
    flowControls->addWidget(flowChaos_);
    flowControls->addStretch();
    auto openFlow = button(flowControls, "Open selected…", "storylineOpenConnection");
    flowLayout->addLayout(flowControls);
    flow_ = new QTreeWidget;
    flow_->setObjectName("storylineFlow");
    flow_->setHeaderLabels({"World location → event → cutscene", "Condition / connection"});
    flow_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    flow_->setWordWrap(true);
    flowLayout->addWidget(flow_, 1);
    story_->addTab(flowPage, "Flow");
    auto branches = new QWidget;
    auto branchLayout = new QVBoxLayout(branches);
    branchNode_ = new QComboBox;
    branchNode_->setObjectName("storylineNode");
    branchLayout->addWidget(branchNode_);
    auto unlockLocation=button(branchLayout,"Unlock location…","storylineUnlockLocation");
    connect(unlockLocation,&QPushButton::clicked,this,&CampaignEditor::editUnlocks);
    rules_ = new QTableWidget(0, 3);
    rules_->setObjectName("storylineRules");
    rules_->setHorizontalHeaderLabels({"When", "Enter event", "Source"});
    rules_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    rules_->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
    rules_->setWordWrap(true);
    rules_->setSelectionBehavior(QAbstractItemView::SelectRows);
    rules_->setSelectionMode(QAbstractItemView::SingleSelection);
    rules_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto branchSplit=new QSplitter;branchSplit->addWidget(rules_);
    branchDetails_=new QScrollArea;branchDetails_->setObjectName("storylineBranchDetails");branchDetails_->setWidgetResizable(true);branchSplit->addWidget(branchDetails_);
    branchSplit->setStretchFactor(0,3);branchSplit->setStretchFactor(1,2);branchSplit->setSizes({900,600});branchLayout->addWidget(branchSplit,1);
    auto ruleActions = new QHBoxLayout;
    auto addRule = button(ruleActions, "Add branch…", "storylineAddBranch"),
         editRuleButton = button(ruleActions, "Edit branch…", "storylineEditBranch"),
         removeRule = button(ruleActions, "Remove branch", "storylineRemoveBranch"),
         openEvent = button(ruleActions, "Open destination event", "storylineOpenEvent");
    branchLayout->addLayout(ruleActions);
    routeInfo_ = new QLabel;
    routeInfo_->setObjectName("storylineOriginalRules");
    routeInfo_->setWordWrap(true);
    routeInfo_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    branchLayout->addWidget(routeInfo_);
    story_->addTab(branches, "Branches");
    events_ = new ScenarioEventEditor(p, undo, changed_);
    events_->showAllRows();
    events_->setObjectName("storylineEvents");
    for (auto child : events_->findChildren<QObject *>())
        if (child->objectName().startsWith("scenarioEvent"))
            child->setObjectName(
                QString(child->objectName()).replace("scenarioEvent", "storylineEvent"));
    story_->addTab(events_, "Event rows");
    auto cutscenes = new CutsceneEditor(p->rom(), nullptr, p, undo, changed_);
    story_->addTab(cutscenes, "Cutscenes");
    const int storylineTab=tabs_->addTab(story_, "Storyline");
    auto badgeArea = new QWidget;
    auto badgeLayout = new QHBoxLayout(badgeArea);
    // The themed tab's outer margin is part of its rectangle, so keep the
    // badge inset from that edge instead of filling the native button slot.
    badgeLayout->setContentsMargins(0, 0, 12, 0);
    auto badge = new QLabel("W.I.P");
    badge->setObjectName("storylineWipBadge");
    badge->setStyleSheet("color: #b51f1f; background: #f1d99c; font-weight: bold; border-radius: 3px; padding: 2px 5px;");
    badge->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    badge->setAttribute(Qt::WA_TransparentForMouseEvents);
    badgeLayout->addWidget(badge);
    badgeArea->setAttribute(Qt::WA_TransparentForMouseEvents);
    badgeArea->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    badgeArea->adjustSize();
    tabs_->tabBar()->setTabButton(storylineTab,QTabBar::RightSide,badgeArea);
    tabs_->setCurrentWidget(scenarios_);
    events_->editCutscene = [this](int selector) { openCutscene(selector); };
    scenarios_->openStoryline = [this](int key) {
        const auto rows = project_->sceneEvents()->scenarioRows(key);
        tabs_->setCurrentWidget(story_);
        story_->setCurrentIndex(0);
        refreshFlow();
        for (int i = 0; i < flow_->topLevelItemCount(); ++i) {
            auto node = flow_->topLevelItem(i);
            for (int j = 0; j < node->childCount(); ++j)
                if (rows.contains(node->child(j)->data(0, Qt::UserRole + 1).toInt())) {
                    node->setExpanded(true);
                    node->child(j)->setExpanded(true);
                    flow_->setCurrentItem(node->child(j));
                    flow_->scrollToItem(node->child(j));
                    return;
                }
        }
    };
    for (auto child : scenarios_->findChildren<QWidget *>())
        if (auto events = dynamic_cast<ScenarioEventEditor *>(child))
            events->editCutscene = [this](int selector) { openCutscene(selector); };
    connect(flowChaos_, &QSpinBox::valueChanged, this, [this] {
        if (!loading_)
            refreshFlow();
    });
    connect(story_, &QTabWidget::currentChanged, this, [this](int index) {
        if (!loading_ && index == 0)
            refreshFlow();
    });
    connect(tabs_, &QTabWidget::currentChanged, this, [this] {
        if (!loading_ && tabs_->currentWidget() == story_ && story_->currentIndex() == 0)
            refreshFlow();
    });
    auto follow = [this] {
        auto item = flow_->currentItem();
        if (!item)
            return;
        const int id = item->data(0, Qt::UserRole + 1).toInt();
        const auto kind = item->data(0, Qt::UserRole).toString();
        if (kind == "event")
            selectEvent(id);
        else if (kind == "scene")
            openCutscene(id);
        else if (kind == "scenario") {
            tabs_->setCurrentWidget(scenarios_);
            scenarios_->selectScenario(id);
            scenarios_->findChild<QTabBar *>("scenarioNavigation")->setCurrentIndex(0);
        } else if (kind == "node") {
            branchNode_->setCurrentIndex(branchNode_->findData(id));
            story_->setCurrentIndex(1);
        }
    };
    connect(openFlow, &QPushButton::clicked, this, follow);
    connect(flow_, &QTreeWidget::itemDoubleClicked, this, [follow] { follow(); });
    notice_ = new QLabel;
    notice_->setObjectName("campaignNotice");
    notice_->setWordWrap(true);
    layout->addWidget(notice_);
    canvas_->roadMessage=[this](const QString &message){notice_->setText(message);};
    connect(nodes_, &QListWidget::currentRowChanged, this, [this] {
        if (!loading_) {
            if(nodes_->currentItem())preferredNode_=nodes_->currentItem()->data(Qt::UserRole).toInt();
            showNode();
        }
    });
    connect(nodes_, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        preferredNode_=item->data(Qt::UserRole).toInt();
    });
    connect(branchNode_, &QComboBox::currentIndexChanged, this, [this] {
        if (!loading_)
            showBranches();
    });
    connect(roads_, &QListWidget::currentRowChanged, this, [this,removeRoad] {
        canvas_->selectedRoad=roads_->currentItem()?roads_->currentItem()->data(Qt::UserRole).toInt():0;
        removeRoad->setEnabled(canvas_->selectedRoad>0);canvas_->update();
    });
    connect(create, &QPushButton::clicked, this, &CampaignEditor::createNode);
    canvas_->placementModeChanged=[this,create](bool active) {
        create->setText(active?"Cancel placement":"Create map location");
        notice_->setText(active?"Click the map to place a location. Esc or right-click to cancel.":QString());
    };
    canvas_->locationPlaced=[this](int id,QPoint position) {
        try {
            auto map=*project_->campaignMap();
            if(!map.availableNodes().contains(id))throw Error("This map location slot is already in use");
            const int label=map.label(id)>0?map.label(id):map.labels().firstKey();
            map.setNode(id,position,label);map.setName(id,"New map location");
            canvas_->cancelPlacement();commitMap(map,"Create map location");selectNode(id);
            label_->setFocus();label_->selectAll();
        }catch(const Error &e){notice_->setText(QString::fromUtf8(e.what()));}
    };
    connect(apply, &QPushButton::clicked, this, [this] {
        if (!nodes_->currentItem())
            return;
        try {
            auto map = *project_->campaignMap();
            const int id=nodes_->currentItem()->data(Qt::UserRole).toInt();
            map.setNode(id,{x_->value(), y_->value()},map.label(id));map.setName(id,label_->text());
            commitMap(map, "Edit map location");
        } catch (const Error &e) {
            notice_->setText(QString::fromUtf8(e.what()));
        }
    });
    connect(icon_,&QComboBox::activated,this,[this] {
        if(loading_ || !nodes_->currentItem())return;
        try {auto map=*project_->campaignMap();map.setIcon(nodes_->currentItem()->data(Qt::UserRole).toInt(),icon_->currentData().toInt());commitMap(map,"Change location icon");}
        catch(const Error &e){notice_->setText(QString::fromUtf8(e.what()));showNode();}
    });
    canvas_->connectionModeChanged=[this,addRoad](bool active) {
        addRoad->setText(active ? "Cancel connection" : "Add connection");
        notice_->setText(active ? "Click the destination map location. Esc or right-click to cancel." : QString());
    };
    connect(addRoad,&QPushButton::clicked,this,[this] {
        if (canvas_->choosingConnection()) { canvas_->cancelConnection(); return; }
        if (!nodes_->currentItem()) return;
        const int from=nodes_->currentItem()->data(Qt::UserRole).toInt();
        const auto &map=*project_->campaignMap();
        for(int to=1;to<=CampaignMap::nodeCount;++to) {
            if(to==from || !map.position(to).x())continue;
            bool connected=false;
            for(int r=1;r<=map.totalRoads();++r) connected|=map.road(r)==qMakePair(from,to)||map.road(r)==qMakePair(to,from);
            if(!connected){canvas_->beginConnection(from);return;}
        }
        notice_->setText("No unconnected locations remain.");
    });
    canvas_->connectionPicked=[this](int from,int to) {
        try {
            auto map=*project_->campaignMap();const int id=map.addRoad(from,to);
            canvas_->cancelConnection();commitMap(map,"Add world-map connection");
            for(int row=0;row<roads_->count();++row)if(roads_->item(row)->data(Qt::UserRole).toInt()==id)roads_->setCurrentRow(row);
        } catch(const Error &e){notice_->setText(QString::fromUtf8(e.what()));}
    };
    connect(removeRoad,&QPushButton::clicked,this,[this] {
        if(!roads_->currentItem())return;
        try {
            auto map=*project_->campaignMap();
            map.removeRoad(roads_->currentItem()->data(Qt::UserRole).toInt());
            commitMap(map,"Remove world-map connection");
        } catch(const Error &e){notice_->setText(QString::fromUtf8(e.what()));}
    });
    connect(branch, &QPushButton::clicked, this, [this] {
        if (!nodes_->currentItem())
            return;
        branchNode_->setCurrentIndex(
            branchNode_->findData(nodes_->currentItem()->data(Qt::UserRole)));
        tabs_->setCurrentWidget(story_);
        story_->setCurrentIndex(1);
    });
    connect(addRule, &QPushButton::clicked, this, [this] { editRule(-1); });
    connect(editRuleButton, &QPushButton::clicked, this, [this] {
        if (rules_->currentRow() >= 0)
            editRule(rules_->currentRow());
    });
    connect(rules_,&QTableWidget::cellDoubleClicked,this,[this](int row,int){editRule(row);});
    connect(removeRule, &QPushButton::clicked, this, [this] {
        const int row = rules_->currentRow(), node = branchNode_->currentData().toInt();
        if (row < 0 || rules_->item(row,0)->data(Qt::UserRole).toInt()<0)
            return;
        try {
            auto next = *project_;
            auto events = *next.sceneEvents();
            auto a = events.flagEdits()["0"].toObject()[QString::number(node)].toArray();
            a.removeAt(row);
            events.setWorldRules(node, a);
            next.setSceneEvents(events.json());
            commit(std::move(next), "Remove story branch");
        } catch (const Error &e) {
            notice_->setText(QString::fromUtf8(e.what()));
        }
    });
    connect(openEvent, &QPushButton::clicked, this, [this] {
        const int row = rules_->currentRow();
        if (row < 0)
            return;
        selectEvent(rules_->item(row, 1)->data(Qt::UserRole).toInt());
    });
    connect(rules_,&QTableWidget::itemSelectionChanged,this,[this,editRuleButton,removeRule,openEvent] {
        const int row=rules_->currentRow();const auto item=row>=0?rules_->item(row,0):nullptr;
        const bool custom=item && item->data(Qt::UserRole).toInt()>=0;
        editRuleButton->setEnabled(item!=nullptr);editRuleButton->setText(custom?"Edit branch…":"Edit entry conditions…");removeRule->setEnabled(custom);openEvent->setEnabled(item!=nullptr);
        showBranchConditions();
    });
    canvas_->roadPicked=[this](int id) {
        const auto ends=project_->campaignMap()->road(id);
        const int node=nodes_->currentItem()?nodes_->currentItem()->data(Qt::UserRole).toInt():0;
        if (node!=ends.first && node!=ends.second)selectNode(ends.first);
        for(int row=0;row<roads_->count();++row)if(roads_->item(row)->data(Qt::UserRole).toInt()==id)roads_->setCurrentRow(row);
    };
    canvas_->roadShaped=[this](int road,const QVector<QPoint> &points) {
        try {
            auto map=*project_->campaignMap();map.setRoadDots(road,points);
            commitMap(map,"Reshape world-map road");
        } catch(const Error &e){notice_->setText(QString::fromUtf8(e.what()));}
    };
    canvas_->picked = [this](int node) { selectNode(node); };
    canvas_->moved = [this](int node, QPoint point) {
        try {
            auto map = *project_->campaignMap();
            map.setNode(node, point, map.label(node));
            commitMap(map, "Move map location");
        } catch (const Error &e) {
            notice_->setText(QString::fromUtf8(e.what()));
        }
    };
    watchContentChanges(undo_, this, [this] {
        refresh();
        changed_();
    }, true);
    refresh();
}
QString CampaignEditor::eventName(int row) const {
    QStringList names;
    for (int key : routes_.keys.value(row))
        if (project_->scenarios()->entries().contains(key))
            names << project_->scenarios()->scenario(key).name;
    return QString("Event row %1%2")
        .arg(row)
        .arg(names.isEmpty() ? QString() : " · " + names.join(" / "));
}
QComboBox *CampaignEditor::eventPicker() const {
    auto box = new QComboBox;
    box->setMinimumContentsLength(42);
    box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    for (int row = 4; row < 115; ++row)
        if (routes_.scripts.contains(row))
            box->addItem(eventName(row), row);
    return box;
}
void CampaignEditor::refresh() {
    loading_ = true;
    const int selected = preferredNode_;
    const int branch = branchNode_->currentData().toInt();
    nodes_->clear();
    branchNode_->clear();

    const auto &map = *project_->campaignMap();
    {const QSignalBlocker block(icon_);icon_->clear();
        for(int id:map.iconIds())icon_->addItem(QIcon(QPixmap::fromImage(map.iconFrame(id,1,0).scaled(32,32))),map.iconName(id),id);
    }
    for (int id = 1; id <= CampaignMap::nodeCount; ++id)
        if (map.position(id).x()) {
            const auto text = QString("%1 · %2").arg(id).arg(map.name(id));
            auto item = new QListWidgetItem(text, nodes_);
            item->setData(Qt::UserRole, id);
            if (id == selected)
                nodes_->setCurrentItem(item);
            branchNode_->addItem(text, id);

        }
    if (!nodes_->currentItem() && nodes_->count())
        nodes_->setCurrentRow(0);
    branchNode_->setCurrentIndex(qMax(0, branchNode_->findData(branch)));
    auto create = findChild<QPushButton *>("campaignCreateNode");
    create->setEnabled(!map.availableNodes().isEmpty());
    create->setToolTip(map.availableNodes().isEmpty() ? "No unused map location slots remain." : QString());
    loading_ = false;
    showNode();
    showBranches();
    if (story_->isVisible() && story_->currentIndex() == 0)
        refreshFlow();
}
void CampaignEditor::showNode() {
    canvas_->cancelConnection();
    canvas_->cancelPlacement();
    if (!nodes_->currentItem())
        return;
    const int node = nodes_->currentItem()->data(Qt::UserRole).toInt();
    const auto &map = *project_->campaignMap();
    {
        const QSignalBlocker blocked(roads_);
        const int previous=canvas_->selectedRoad;roads_->clear();
        for(int id=1;id<=map.totalRoads();++id) {
            const auto ends=map.road(id);
            if(ends.first==node || ends.second==node) {
                auto item=new QListWidgetItem(map.name(ends.first==node?ends.second:ends.first),roads_);
                item->setData(Qt::UserRole,id);
                if(id==previous)roads_->setCurrentItem(item);
            }
        }
        if(!roads_->currentItem() && roads_->count())roads_->setCurrentRow(0);
        canvas_->selectedRoad=roads_->currentItem()?roads_->currentItem()->data(Qt::UserRole).toInt():0;
        findChild<QPushButton *>("campaignRemoveRoad")->setEnabled(canvas_->selectedRoad>0);
    }
    x_->setValue(map.position(node).x());
    y_->setValue(map.position(node).y());
    label_->setText(map.name(node));
    description_->selectDescription("locations", node - 1);
    {const QSignalBlocker block(icon_);icon_->setCurrentIndex(icon_->findData(map.icon(node)));}
    canvas_->selected = node;
    canvas_->syncMap();
    // Refreshing an edited event must not navigate Storyline back to the map's
    // last selection. Follow locations only while the user is on World Map.
    if(tabs_->currentIndex()==0)events_->selectLocation(node);
}
void CampaignEditor::selectNode(int node) {
    for (int row = 0; row < nodes_->count(); ++row)
        if (nodes_->item(row)->data(Qt::UserRole).toInt() == node) {
            preferredNode_=node;
            tabs_->setCurrentIndex(0);
            nodes_->setCurrentRow(row);
            showNode();
            return;
        }
}
void CampaignEditor::selectEvent(int row, int offset) {
    tabs_->setCurrentWidget(story_);
    story_->setCurrentWidget(events_);
    events_->selectEventRow(row, offset);
}
void CampaignEditor::openCutscene(int selector) {
    for (const auto &id : project_->scenePrograms()->scenes())
        if (project_->scenePrograms()->resource(id).selectors.contains(selector)) {
            tabs_->setCurrentWidget(story_);
            story_->setCurrentIndex(3);
            auto search = findChild<QLineEdit *>("cutsceneScenesSearch");
            if (search)
                search->clear();
            auto scenes = findChild<QListWidget *>("cutsceneScenes");
            if (!scenes)
                return;
            for (int row = 0; row < scenes->count(); ++row)
                if (scenes->item(row)->data(Qt::UserRole).toUInt() == id.nativeKey &&
                    scenes->item(row)->data(Qt::UserRole + 1).toString() == id.authoredId) {
                    scenes->setCurrentRow(row);
                    scenes->scrollToItem(scenes->item(row));
                    return;
                }
        }
}
void CampaignEditor::refreshFlow() {
    flow_->clear();
    auto routes = routes_;
    routes.scripts[0] = project_->sceneEvents()->program(0);
    for (int node = 1; node <= CampaignMap::nodeCount; ++node) {
        const auto &map = *project_->campaignMap();
        if (!map.position(node).x())
            continue;
        routes.nodes[node] = {map.name(node), map.position(node)};
        auto parent = new QTreeWidgetItem(
            flow_, {QString("%1 · %2").arg(node).arg(map.name(node)), "World-map entry"});
        parent->setData(0, Qt::UserRole, "node");
        parent->setData(0, Qt::UserRole + 1, node);
        try {
            for (const auto &route : routes.routes(node, flowChaos_->value())) {
                QStringList checks;
                for (auto it = route.requirements.cbegin(); it != route.requirements.cend(); ++it)
                    checks << EventScript::flagName(it.key()) + (it.value() ? " ON" : " OFF");
                auto event = new QTreeWidgetItem(
                    parent,
                    {eventName(route.event),
                     checks.isEmpty() ? QString("Always at this Chaos Frame") : checks.join(", ")});
                event->setData(0, Qt::UserRole, "event");
                event->setData(0, Qt::UserRole + 1, route.event);
                for (int key : routes_.keys.value(route.event))
                    if (project_->scenarios()->entries().contains(key)) {
                        auto scenario = new QTreeWidgetItem(
                            event, {project_->scenarios()->scenario(key).name, "Scenario map"});
                        scenario->setData(0, Qt::UserRole, "scenario");
                        scenario->setData(0, Qt::UserRole + 1, key);
                    }
                for (const auto &site : SceneEvents::sites())
                    if (site.row == route.event) {
                        const int selector = project_->sceneEvents()->target(site.id);
                        auto scene = new QTreeWidgetItem(event, {site.name, site.role});
                        scene->setData(0, Qt::UserRole, "scene");
                        scene->setData(0, Qt::UserRole + 1, selector);
                    }
            }
        } catch (const Error &e) {
            new QTreeWidgetItem(parent, {"Route preview unavailable", QString::fromUtf8(e.what())});
        }
    }
}
void CampaignEditor::showBranches() {
    const int node = branchNode_->currentData().toInt();
    const bool sameNode=branchDetailsNode_==node;
    const int previous=sameNode?rules_->currentRow():0;branchDetailsNode_=node;
    const auto previousKey=sameNode&&branchEntries_.contains(previous)?entryIdentity(branchEntries_[previous]):QString{};
    const QSignalBlocker block(rules_);branchEntries_.clear();branchEntryErrors_.clear();
    if(node>0&&story_->isVisible()&&story_->currentIndex()==1)events_->selectLocation(node);
    const auto rules =
        project_->sceneEvents()->flagEdits()["0"].toObject()[QString::number(node)].toArray();
    rules_->setRowCount(rules.size());
    for (int i = 0; i < rules.size(); ++i) {
        const auto rule = rules[i].toObject();
        auto condition=new QTableWidgetItem(conditions(rule["conditions"].toArray()));condition->setData(Qt::UserRole,i);
        rules_->setItem(i, 0, condition);
        auto item = new QTableWidgetItem(eventName(rule["event"].toInt()));
        item->setData(Qt::UserRole, rule["event"]);
        rules_->setItem(i, 1, item);
        rules_->setItem(i,2,new QTableWidgetItem("Added"));
    }
    routeInfo_->clear();routeInfo_->hide();
    try {
        auto native=routes_;const auto retail=SceneEvents::retailRow(project_->rom(),0);
        native.scripts[0]=EventScript::applyOperands(retail,0,project_->sceneEvents()->flagEdits()["0"].toObject()["operands"].toObject());
        QVector<StoryRoute> original;QVector<QVector<int>> chaosValues;
        for(int chaos=0;chaos<=100;++chaos)for(const auto &route:native.routes(node,chaos)) {
            int index=original.indexOf(route);
            if(index<0){index=original.size();original<<route;chaosValues.append(QVector<int>{});}
            chaosValues[index]<<chaos;
        }
        for(int index=0;index<original.size();++index) {
            const auto &route=original[index];
            QStringList checks;
            for (auto it = route.requirements.cbegin(); it != route.requirements.cend(); ++it)
                checks << EventScript::flagName(it.key()) + (it.value() ? " = ON" : " = OFF");
            if(chaosValues[index].size()!=101) {
                QStringList ranges;const auto &values=chaosValues[index];
                for(int i=0;i<values.size();) {
                    const int first=values[i];int last=first;
                    while(++i<values.size() && values[i]==last+1)last=values[i];
                    ranges<< (first==last?QString::number(first):QString("%1–%2").arg(first).arg(last));
                }
                checks<<"Chaos Frame "+ranges.join(", ");
            }
            const int row=rules_->rowCount();rules_->insertRow(row);
            auto when=new QTableWidgetItem(checks.isEmpty()?"Always":checks.join("; "));when->setData(Qt::UserRole,-1);
            auto event=new QTableWidgetItem(eventName(route.event));event->setData(Qt::UserRole,route.event);
            rules_->setItem(row,0,when);rules_->setItem(row,1,event);rules_->setItem(row,2,new QTableWidgetItem(native.scripts[0]==retail?"Game":"Edited game logic"));
            try {branchEntries_[row]=EntryConditions::read(project_->rom(),*project_->sceneEvents(),*project_->scenePrograms(),route,chaosValues[index].first());}
            catch(const Error &e){branchEntryErrors_[row]=QString::fromUtf8(e.what());}
        }
    } catch (const Error &e) {
        routeInfo_->setText(QString::fromUtf8(e.what()));routeInfo_->show();
    }
    int selectedRow=qBound(0,previous,rules_->rowCount()-1);
    if(!previousKey.isEmpty())for(auto entry=branchEntries_.begin();entry!=branchEntries_.end();++entry)if(entryIdentity(entry.value())==previousKey){selectedRow=entry.key();break;}
    if(rules_->rowCount())rules_->setCurrentCell(selectedRow,0);
    else if(routeInfo_->text().isEmpty()){routeInfo_->setText("No entry branches for this location.");routeInfo_->show();}
    const auto selected=rules_->currentRow()>=0?rules_->item(rules_->currentRow(),0):nullptr;
    const bool custom=selected && selected->data(Qt::UserRole).toInt()>=0;
    findChild<QPushButton *>("storylineEditBranch")->setEnabled(selected!=nullptr);
    findChild<QPushButton *>("storylineEditBranch")->setText(custom?"Edit branch…":"Edit entry conditions…");
    findChild<QPushButton *>("storylineRemoveBranch")->setEnabled(custom);
    findChild<QPushButton *>("storylineOpenEvent")->setEnabled(selected!=nullptr);
    rules_->resizeRowsToContents();
    showBranchConditions();
}
void CampaignEditor::changeBranch(std::function<void(SceneEvents &)> edit,const QString &label) {
    QTimer::singleShot(0,this,[this,edit=std::move(edit),label]{try{
        auto next=*project_;auto events=*next.sceneEvents();edit(events);next.setSceneEvents(events.json());commit(std::move(next),label);
    }catch(const std::exception &e){notice_->setText(QString::fromUtf8(e.what()));}});
}
void CampaignEditor::showBranchConditions() {
    if(auto old=branchDetails_->takeWidget()){old->hide();old->deleteLater();}
    auto content=new QWidget;auto layout=new QVBoxLayout(content);branchDetails_->setWidget(content);
    const int row=rules_->currentRow(),node=branchNode_->currentData().toInt();
    if(row<0||!rules_->item(row,0)){layout->addWidget(new QLabel("Select a branch to edit its conditions."));layout->addStretch();return;}
    auto heading=new QLabel("Entry conditions");auto bold=heading->font();bold.setBold(true);heading->setFont(bold);layout->addWidget(heading);
    const int custom=rules_->item(row,0)->data(Qt::UserRole).toInt();
    auto makeFlag=[&](int id,int minimum,int maximum){auto box=new QComboBox;box->setObjectName(QString("branchFlag_%1").arg(id));box->setEditable(true);box->setInsertPolicy(QComboBox::NoInsert);box->setMinimumContentsLength(20);box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        for(int flag=minimum;flag<=maximum;++flag)box->addItem(project_->sceneEvents()->flagName(flag,0),flag);box->setCurrentIndex(box->findData(id));box->completer()->setFilterMode(Qt::MatchContains);box->completer()->setCaseSensitivity(Qt::CaseInsensitive);box->lineEdit()->setCursorPosition(0);return box;};
    auto makeState=[&](int flag,int value){auto box=new QComboBox;box->setObjectName(QString("branchFlagState_%1").arg(flag));box->addItem("OFF",0);box->addItem("ON",1);box->setCurrentIndex(box->findData(value));return box;};
    auto makeCheck=[&](int number){auto group=new QGroupBox(QString("IF %1").arg(number));auto form=new QFormLayout(group);form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);layout->addWidget(group);return form;};
    auto links=[&](QFormLayout *form,int flag){form->addRow(eventFlagLinks(project_,0,flag,[this](int r,int at){selectEvent(r,at);}));};
    if(custom>=0) {
        const auto all=project_->sceneEvents()->flagEdits()["0"].toObject()[QString::number(node)].toArray();
        if(custom>=all.size())return;const auto rule=all[custom].toObject();const auto conditions=rule["conditions"].toArray();
        for(int i=0;i<conditions.size();++i){const auto c=conditions[i].toObject();const int flag=c["flag"].toInt();auto form=makeCheck(i+1);auto flagBox=makeFlag(flag,0,255);auto state=makeState(flag,c["value"].toBool());form->addRow("Flag",flagBox);form->addRow("State",state);links(form,flag);
            auto update=[=,this](const QString &key,QJsonValue value){changeBranch([=](SceneEvents &events){auto rules=events.flagEdits()["0"].toObject()[QString::number(node)].toArray();auto r=rules[custom].toObject();auto tests=r["conditions"].toArray();auto test=tests[i].toObject();test[key]=value;tests[i]=test;r["conditions"]=tests;rules[custom]=r;events.setWorldRules(node,rules);},"Edit entry condition");};
            connect(flagBox,&QComboBox::activated,this,[=](int at){update("flag",flagBox->itemData(at).toInt());});connect(state,&QComboBox::activated,this,[=](int at){update("value",state->itemData(at).toInt()!=0);});
        }
        if(conditions.isEmpty())layout->addWidget(new QLabel("Always"));
        auto destination=eventPicker();destination->setObjectName("branchDestination");destination->setCurrentIndex(destination->findData(rule["event"].toInt()));layout->addWidget(new QLabel("THEN enter"));layout->addWidget(destination);
        connect(destination,&QComboBox::activated,this,[=,this](int at){const int event=destination->itemData(at).toInt();changeBranch([=](SceneEvents &events){auto rules=events.flagEdits()["0"].toObject()[QString::number(node)].toArray();auto r=rules[custom].toObject();r["event"]=event;rules[custom]=r;events.setWorldRules(node,rules);},"Change branch destination");});
        auto edit=new QPushButton("Add or remove conditions…");layout->addWidget(edit);connect(edit,&QPushButton::clicked,this,[this,row]{editRule(row);});
    }else if(branchEntries_.contains(row)) {
        const auto entry=branchEntries_[row];int number=0;
        for(const auto &condition:entry.flags){auto form=makeCheck(++number);auto flag=makeFlag(condition.flag,condition.minimum,condition.maximum);auto state=makeState(condition.flag,condition.value);form->addRow("Flag",flag);form->addRow("State",state);links(form,condition.flag);
            flag->setEnabled(condition.unavailable.isEmpty()&&!condition.flagOffsets.isEmpty());state->setEnabled(condition.stateEditable);
            if(!condition.unavailable.isEmpty()){auto info=new QLabel(condition.unavailable);info->setWordWrap(true);form->addRow(info);}
            bool shared=false;for(auto other=branchEntries_.begin();other!=branchEntries_.end();++other)if(other.key()!=row)for(const auto &check:other->flags)for(int at:condition.comparisonOffsets)shared|=check.comparisonOffsets.contains(at);
            if(shared){auto info=new QLabel("Shared check — edits also update the other branches using it.");info->setWordWrap(true);form->addRow(info);}
            connect(flag,&QComboBox::activated,this,[=,this](int at){const int value=flag->itemData(at).toInt();changeBranch([=,this](SceneEvents &events){EntryConditions::setFlag(events,*project_->scenePrograms(),condition,value);},"Change entry flag");});
            connect(state,&QComboBox::activated,this,[=,this](int at){const int value=state->itemData(at).toInt();changeBranch([=,this](SceneEvents &events){EntryConditions::setState(events,*project_->scenePrograms(),condition,value);},"Change entry flag state");});
        }
        for(const auto &condition:entry.numbers){auto form=makeCheck(++number);auto relation=new QComboBox;relation->setObjectName(QString("branchNumberRelation_%1").arg(condition.comparisonOffset));
            for(auto [id,name]:QMap<int,QString>{{10,"Equals"},{11,"Does not equal"},{12,"Greater than"},{13,"Less than"},{14,"At least"},{15,"At most"}}.asKeyValueRange())relation->addItem(name,id);relation->setCurrentIndex(relation->findData(condition.relation));
            auto value=new QSpinBox;value->setObjectName(QString("branchNumberValue_%1").arg(condition.valueOffset));value->setRange(condition.minimum,condition.maximum);value->setValue(condition.value);value->setKeyboardTracking(false);form->addRow(condition.name,relation);form->addRow("Value",value);
            connect(relation,&QComboBox::activated,this,[=,this](int at){const int selected=relation->itemData(at).toInt();changeBranch([=,this](SceneEvents &events){EntryConditions::setNumberRelation(events,*project_->scenePrograms(),condition,selected);},"Change entry comparison");});
            connect(value,&QSpinBox::valueChanged,this,[=,this](int selected){changeBranch([=,this](SceneEvents &events){EntryConditions::setNumberValue(events,*project_->scenePrograms(),condition,selected);},"Change entry value");});
        }
        if(!number)layout->addWidget(new QLabel("Always"));
        auto destination=eventPicker();destination->setObjectName("branchDestination");destination->setCurrentIndex(destination->findData(entry.event));layout->addWidget(new QLabel("THEN enter"));layout->addWidget(destination);
        connect(destination,&QComboBox::activated,this,[=,this](int at){const int value=destination->itemData(at).toInt();changeBranch([=,this](SceneEvents &events){EntryConditions::setEvent(events,*project_->scenePrograms(),entry,value);},"Change branch destination");});
    }else{auto error=new QLabel(branchEntryErrors_.value(row,"This entry has no editable conditions."));error->setWordWrap(true);layout->addWidget(error);}
    auto full=new QPushButton("Show full entry logic");layout->addWidget(full);connect(full,&QPushButton::clicked,this,[this,node]{events_->selectLocation(node);events_->setRawInstructions(false);selectEvent(0,u16(SceneEvents::retailRow(project_->rom(),0),node*2));});
    layout->addStretch();
}
void CampaignEditor::commit(Project after, const QString &label) {
    if (after.manifest() != project_->manifest())
        undo_->push(new CampaignCommand(project_, std::move(after), label));
    notice_->setText(label + " · Applied");
}
void CampaignEditor::commitMap(const CampaignMap &map,const QString &label) {
    if(map.json()!=project_->campaignMap()->json())undo_->push(new MapCommand(project_,map,label));
    notice_->setText(label + " · Applied");
}
void CampaignEditor::editRule(int row) {
    const int node = branchNode_->currentData().toInt();
    if(row>=0&&row<rules_->rowCount()&&rules_->item(row,0)->data(Qt::UserRole).toInt()<0){
        showBranchConditions();branchDetails_->setFocus();return;
    }
    auto rules =
        project_->sceneEvents()->flagEdits()["0"].toObject()[QString::number(node)].toArray();
    if (node < 1 || (row >= 0 && row >= rules.size()))
        return;
    const auto current =
        row >= 0
            ? rules[row].toObject()
            : QJsonObject{
                  {"event", 4}, {"clearFrom", 200}, {"clearTo", 255}, {"conditions", QJsonArray{}}};
    QDialog dialog(this);
    dialog.setWindowTitle(row < 0 ? "Add story branch" : "Edit story branch");
    auto layout = new QVBoxLayout(&dialog);
    auto form = new QFormLayout;
    auto event = eventPicker();
    event->setObjectName("storyBranchDestination");
    event->setCurrentIndex(event->findData(current["event"].toInt()));
    form->addRow("Enter", event);
    layout->addLayout(form);
    auto priority = new QSpinBox;
    priority->setObjectName("storyBranchPriority");
    priority->setRange(1, rules.size() + (row < 0 ? 1 : 0));
    priority->setValue(row < 0 ? 1 : row + 1);
    form->addRow("Priority (first match wins)", priority);
    auto table = new QTableWidget(0, 2);
    table->setObjectName("storyBranchConditions");
    table->setHorizontalHeaderLabels({"Flag", "Required value"});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    layout->addWidget(table);
    auto addCondition = [table](int id, bool on) {
        int r = table->rowCount();
        table->insertRow(r);
        auto flag = new QComboBox;
        for (int f = 0; f < 256; ++f)
            flag->addItem(EventScript::flagName(f), f);
        flag->setCurrentIndex(id);
        table->setCellWidget(r, 0, flag);
        auto value = new QComboBox;
        value->addItems({"OFF", "ON"});
        value->setCurrentIndex(on);
        table->setCellWidget(r, 1, value);
    };
    for (auto v : current["conditions"].toArray()) {
        auto c = v.toObject();
        addCondition(c["flag"].toInt(), c["value"].toBool());
    }
    auto actions = new QHBoxLayout;
    auto add = button(actions, "Add condition", "storyBranchAddCondition"),
         remove = button(actions, "Remove condition", "storyBranchRemoveCondition");
    layout->addLayout(actions);
    connect(add, &QPushButton::clicked, &dialog, [=] {
        if (table->rowCount() < 16)
            addCondition(0, true);
    });
    connect(remove, &QPushButton::clicked, &dialog, [=] {
        if (table->currentRow() >= 0)
            table->removeRow(table->currentRow());
    });
    auto info =
        new QLabel("All listed conditions must match. With no conditions this branch always runs. "
                   "If no custom branch matches, the original game rules run.");
    info->setWordWrap(true);
    layout->addWidget(info);
    auto clearFrom = new QSpinBox, clearTo = new QSpinBox;
    clearFrom->setRange(0, 255);
    clearTo->setRange(0, 255);
    clearFrom->setValue(current["clearFrom"].toInt());
    clearTo->setValue(current["clearTo"].toInt());
    auto advanced = new QFormLayout;
    advanced->addRow("Clear temporary flags from ID", clearFrom);
    advanced->addRow("Stop before ID", clearTo);
    layout->addLayout(advanced);
    auto error = new QLabel;
    error->setWordWrap(true);
    layout->addWidget(error);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        try {
            QJsonArray tests;
            for (int r = 0; r < table->rowCount(); ++r)
                tests.append(QJsonObject{
                    {"flag",
                     qobject_cast<QComboBox *>(table->cellWidget(r, 0))->currentData().toInt()},
                    {"value",
                     bool(qobject_cast<QComboBox *>(table->cellWidget(r, 1))->currentIndex())}});
            QJsonObject rule{{"event", event->currentData().toInt()},
                             {"clearFrom", clearFrom->value()},
                             {"clearTo", clearTo->value()},
                             {"conditions", tests}};
            auto candidate = rules;
            if (row >= 0)
                candidate.removeAt(row);
            candidate.insert(priority->value() - 1, rule);
            auto next = *project_;
            auto edits = *next.sceneEvents();
            edits.setWorldRules(node, candidate);
            next.setSceneEvents(edits.json());
            commit(std::move(next), "Edit world entry branch");
            dialog.accept();
        } catch (const Error &e) {
            error->setText(QString::fromUtf8(e.what()));
        }
    });
    dialog.resize(660, 450);
    dialog.exec();
}
void CampaignEditor::createNode() {
    if(canvas_->placingLocation()){canvas_->cancelPlacement();return;}
    const auto available=project_->campaignMap()->availableNodes();
    if(!available.isEmpty())canvas_->beginPlacement(available.first());
}
void CampaignEditor::editUnlocks() {
    const int location=branchNode_->currentData().toInt();if(location<1)return;
    QDialog dialog(this);dialog.setWindowTitle("Unlock map location");
    auto layout=new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel("Unlock after completing any selected mission:"));
    auto search=new QLineEdit;search->setPlaceholderText("Find a mission…");layout->addWidget(search);
    auto missions=new QListWidget;missions->setObjectName("mapLocationUnlockMissions");layout->addWidget(missions);
    const auto edits=project_->sceneEvents()->flagEdits();
    for(int row=4;row<115;++row)if(routes_.scripts.contains(row)) {
        auto item=new QListWidgetItem(eventName(row),missions);item->setData(Qt::UserRole,row);
        item->setCheckState(edits[QString::number(row)].toObject()["unlockNodes"].toObject()[QString::number(location)].toBool()?Qt::Checked:Qt::Unchecked);
    }
    connect(search,&QLineEdit::textChanged,&dialog,[=](const QString &text){for(int i=0;i<missions->count();++i)missions->item(i)->setHidden(!missions->item(i)->text().contains(text,Qt::CaseInsensitive));});
    auto error=new QLabel;error->setWordWrap(true);layout->addWidget(error);
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
        try {
            auto next=*project_;auto events=*next.sceneEvents();
            for(int i=0;i<missions->count();++i) {
                const auto item=missions->item(i);const int row=item->data(Qt::UserRole).toInt();
                const bool enabled=item->checkState()==Qt::Checked;
                if(enabled!=edits[QString::number(row)].toObject()["unlockNodes"].toObject()[QString::number(location)].toBool())events.unlockNodeAfter(row,location,enabled);
            }
            next.setSceneEvents(events.json());commit(std::move(next),"Edit map location unlocks");dialog.accept();
        }catch(const Error &e){error->setText(QString::fromUtf8(e.what()));}
    });
    dialog.resize(540,500);dialog.exec();
}

} // namespace lordly
