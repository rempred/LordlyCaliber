#include "app/campaign_art_editor.h"
#include "app/art_image_assignment.h"
#include "app/content_refresh.h"
#include "app/sprite_editor.h"
#include "engine/image_colors.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QHeaderView>
#include <QUndoStack>
#include <QVBoxLayout>
namespace lordly {
namespace {
class ArtworkCommand final:public WorldMapUndoCommand {
public:
    ArtworkCommand(Project *p,const CampaignMap &map,const QString &label):WorldMapUndoCommand(label),p_(p),before_(p->campaignMap()->json()),after_(map.json()){}
    void undo()override{p_->setCampaignMap(before_);}
    void redo()override{p_->setCampaignMap(after_);}
private: Project *p_;QJsonObject before_,after_;
};
void commit(Project *p,QUndoStack *undo,const CampaignMap &map,const QString &text) {
    if(p->campaignMap()->json()!=map.json())undo->push(new ArtworkCommand(p,map,text));
}
}
void replaceCampaignBackground(QWidget *parent,Project *p,QUndoStack *undo) {
    const auto path=QFileDialog::getOpenFileName(parent,"Replace map artwork",{},"Images (*.png *.bmp *.jpg *.jpeg)");
    if(path.isEmpty())return;
    const auto source=loadArtworkImage(path);
    if(source.size()!=QSize(512,320))throw Error("Choose a 512 × 320 image for the world map");
    QDialog dialog(parent);dialog.setObjectName("campaignBackgroundImport");dialog.setWindowTitle("Replace map artwork");
    auto layout=new QVBoxLayout(&dialog);auto views=new QHBoxLayout;layout->addLayout(views);
    QLabel *converted=nullptr;
    for(const auto &title:{QString("Original"),QString("Game result")}) {
        auto column=new QVBoxLayout;column->addWidget(new QLabel(title));auto preview=new QLabel;column->addWidget(preview);views->addLayout(column);
        if(title=="Original")preview->setPixmap(QPixmap::fromImage(source));else converted=preview;
    }
    auto dither=new QCheckBox("Dithering — blend nearby colours with a fine pixel pattern");
    layout->addWidget(dither);layout->addWidget(new QLabel("512 × 320 pixels · 256 colours"));
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("Replace artwork");layout->addWidget(buttons);
    QByteArray prepared;
    auto update=[&]{prepared=CampaignMap::prepareArtwork(source,256,true,dither->isChecked());converted->setPixmap(QPixmap::fromImage(CampaignMap::artworkImage(prepared,{512,320},256)));};
    update();QObject::connect(dither,&QCheckBox::toggled,&dialog,update);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted){auto map=*p->campaignMap();map.setBackground(prepared);commit(p,undo,map,"Replace world-map artwork");}
}
void editCampaignIcons(QWidget *parent,Project *p,QUndoStack *undo,int selected) {
    QDialog dialog(parent);dialog.setWindowTitle("Map location icons");dialog.setObjectName("campaignIconEditor");dialog.resize(850,650);
    auto outer=new QVBoxLayout(&dialog);auto body=new QHBoxLayout;outer->addLayout(body,1);
    auto list=new QListWidget;list->setObjectName("campaignArtworkIcons");list->setIconSize({32,32});list->setMaximumWidth(220);body->addWidget(list);
    auto work=new QVBoxLayout;body->addLayout(work,1);auto form=new QFormLayout;work->addLayout(form);
    auto name=new QLineEdit;name->setObjectName("campaignArtworkName");name->setMaxLength(80);form->addRow("Icon name",name);
    auto state=new QComboBox;state->setObjectName("campaignArtworkState");state->addItems({"Visited · state 1","Mission available · state 2"});form->addRow("Appearance",state);
    auto frame=new QComboBox;frame->setObjectName("campaignArtworkFrame");form->addRow("Frame",frame);
    auto tools=new QHBoxLayout;work->addLayout(tools);
    auto tool=new QComboBox;tool->addItems({"Pencil","Eraser","Fill","Eyedropper"});tools->addWidget(tool);
    auto grid=new QCheckBox("Grid");grid->setChecked(true);tools->addWidget(grid);tools->addStretch();
    auto canvas=new SpriteCanvas;canvas->setObjectName("campaignArtworkCanvas");canvas->setZoom(16);
    canvas->setIndexColor(1);
    auto scroll=new QScrollArea;scroll->setWidget(canvas);scroll->setAlignment(Qt::AlignCenter);work->addWidget(scroll,1);
    auto palette=new QTableWidget(1,16);palette->setObjectName("campaignArtworkPalette");palette->horizontalHeader()->hide();palette->verticalHeader()->hide();
    palette->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);palette->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    palette->setFixedHeight(40);palette->setEditTriggers(QAbstractItemView::NoEditTriggers);work->addWidget(palette);
    auto usage=new QLabel;usage->setWordWrap(true);work->addWidget(usage);
    auto actions=new QHBoxLayout;outer->addLayout(actions);
    auto action=[&](const QString &text,const QString &id){auto b=new QPushButton(text);b->setObjectName(id);actions->addWidget(b);return b;};
    auto add=action("New icon…","campaignArtworkNew"),copy=action("Copy icon…","campaignArtworkCopy"),remove=action("Delete icon","campaignArtworkDelete");
    auto import=action("Replace frame…","campaignArtworkImport"),exportPng=action("Export PNG…","campaignArtworkExport");
    auto undoButton=action("Undo","campaignArtworkUndo"),redoButton=action("Redo","campaignArtworkRedo");
    auto notice=new QLabel;notice->setWordWrap(true);notice->setObjectName("campaignArtworkNotice");outer->addWidget(notice);
    auto close=new QDialogButtonBox(QDialogButtonBox::Close);outer->addWidget(close);QObject::connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    bool loading=false;
    auto guarded=[&](const std::function<void()> &run){try{run();notice->clear();}catch(const std::exception &e){notice->setText(QString::fromUtf8(e.what()));}};
    std::function<void()> refreshFrame,refresh;
    refreshFrame=[&]{
        if(!p->campaignMap()->iconIds().contains(selected))return;
        const auto &map=*p->campaignMap();const int s=state->currentIndex()+1,f=qMax(0,frame->currentIndex());
        const auto data=map.iconFrameBytes(selected,s,f);QVector<QColor> colors;
        for(int n=0;n<16;++n){const auto w=u16(data,n*2);colors<<QColor(((w>>11)*255+15)/31,(((w>>6)&31)*255+15)/31,(((w>>1)&31)*255+15)/31,(w&1)?255:0);auto item=new QTableWidgetItem;item->setBackground(colors.last());item->setToolTip("Double-click to edit this colour");palette->setItem(0,n,item);}
        const int height=map.iconFrame(selected,s,f).height();QByteArray indices(16*height,0);int transparent=0;
        for(int n=0;n<16;++n)if(!colors[n].alpha()){transparent=n;break;}
        for(int n=0;n<indices.size();++n)indices[n]=char((u8(data,32+n/2)>>(n%2?0:4))&15);
        // The shared indexed canvas has256 addressable entries. Location icons
        // expose only their16 real colours; pad its presentation palette.
        while(colors.size()<256)colors<<QColor(Qt::transparent);
        canvas->setIndexedAsset(QString("map-icon-%1-%2-%3").arg(selected).arg(s).arg(f),{16,height},indices,colors,quint8(transparent));
        QStringList users;for(int id=1;id<=40;++id)if(map.icon(id)==selected && map.position(id).x())users<<map.name(id);
        usage->setText(users.isEmpty()?"Not assigned to a map location":"Shared by: "+users.join(", "));
    };
    refresh=[&]{
        loading=true;const auto &map=*p->campaignMap();if(!map.iconIds().contains(selected))selected=0;
        const QSignalBlocker block(list),blockName(name),blockFrame(frame);list->clear();
        for(int id:map.iconIds()){auto item=new QListWidgetItem(QIcon(QPixmap::fromImage(map.iconFrame(id,1,0).scaled(32,32))),map.iconName(id),list);item->setData(Qt::UserRole,id);if(id==selected)list->setCurrentItem(item);}
        name->setText(map.iconName(selected));const int oldFrame=frame->currentIndex();frame->clear();
        for(int n=0;n<(map.iconKind(selected)?1:3);++n)frame->addItem(QString("Frame %1").arg(n+1));
        frame->setCurrentIndex(qBound(0,oldFrame,frame->count()-1));remove->setEnabled(selected>=2);add->setEnabled(map.iconIds().size()<64);copy->setEnabled(add->isEnabled());
        undoButton->setEnabled(undo->canUndo());redoButton->setEnabled(undo->canRedo());loading=false;refreshFrame();
    };
    QObject::connect(list,&QListWidget::currentRowChanged,&dialog,[&]{if(!loading && list->currentItem()){selected=list->currentItem()->data(Qt::UserRole).toInt();refresh();}});
    QObject::connect(name,&QLineEdit::editingFinished,&dialog,[&]{if(!loading)guarded([&]{auto map=*p->campaignMap();map.renameIcon(selected,name->text());commit(p,undo,map,"Rename location icon");});});
    for(auto combo:{state,frame})QObject::connect(combo,&QComboBox::currentIndexChanged,&dialog,[&]{if(!loading)refreshFrame();});
    QObject::connect(tool,&QComboBox::currentIndexChanged,canvas,[&](int n){canvas->setTool(SpriteCanvas::Tool(n));});
    QObject::connect(grid,&QCheckBox::toggled,canvas,&SpriteCanvas::setGrid);
    QObject::connect(palette,&QTableWidget::cellClicked,canvas,[&](int,int column){canvas->setIndexColor(quint8(column));});
    QObject::connect(canvas,&SpriteCanvas::indexPicked,&dialog,[&](int n){palette->setCurrentCell(0,n);canvas->setIndexColor(quint8(n));});
    QObject::connect(palette,&QTableWidget::cellDoubleClicked,&dialog,[&](int,int column){guarded([&]{
        const auto color=QColorDialog::getColor(palette->item(0,column)->background().color(),&dialog,"Icon colour",QColorDialog::ShowAlphaChannel);if(!color.isValid())return;
        auto map=*p->campaignMap();auto data=map.iconFrameBytes(selected,state->currentIndex()+1,frame->currentIndex());
        put16(data,column*2,quint16(((color.red()*31+127)/255<<11)|((color.green()*31+127)/255<<6)|((color.blue()*31+127)/255<<1)|(color.alpha()>=128)));
        map.setIconFrame(selected,state->currentIndex()+1,frame->currentIndex(),data);commit(p,undo,map,"Edit location icon colour");
    });});
    QObject::connect(canvas,&SpriteCanvas::indicesCommitted,&dialog,[&](const QByteArray &indices,QPoint){guarded([&]{
        auto map=*p->campaignMap();auto data=map.iconFrameBytes(selected,state->currentIndex()+1,frame->currentIndex());
        for(int n=0;n<indices.size();++n){const int at=32+n/2,shift=n%2?0:4;data[at]=char((u8(data,at)&~(15<<shift))|((quint8(indices[n])&15)<<shift));}
        map.setIconFrame(selected,state->currentIndex()+1,frame->currentIndex(),data);commit(p,undo,map,"Paint location icon");
    });});
    auto create=[&](bool duplicate){guarded([&]{
        QDialog prompt(&dialog);prompt.setWindowTitle(duplicate?"Copy location icon":"New location icon");auto layout=new QFormLayout(&prompt);
        QLineEdit label;label.setText(duplicate?p->campaignMap()->iconName(selected)+" copy":"New icon");label.setMaxLength(80);layout->addRow("Name",&label);
        QComboBox style;style.addItems({"Animated flag","Static building"});style.setCurrentIndex(1);if(!duplicate)layout->addRow("Style",&style);
        QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addRow(&buttons);
        QObject::connect(&buttons,&QDialogButtonBox::accepted,&prompt,&QDialog::accept);QObject::connect(&buttons,&QDialogButtonBox::rejected,&prompt,&QDialog::reject);
        if(prompt.exec()!=QDialog::Accepted)return;
        auto map=*p->campaignMap();const int id=map.createIcon(label.text(),duplicate?selected:style.currentIndex());
        if(!duplicate) {
            QByteArray blank(160,0);
            const quint16 colors[]={0,0xffff,0x0001,0xf801,0x07c1,0x003f,0xffc1,0xf83f,0x07ff,0x8411,0xc631,0x4209,0xc401,0x8201,0x0411,0x801f};
            for(int n=0;n<16;++n)put16(blank,n*2,colors[n]);
            for(int s=1;s<=2;++s)for(int f=0;f<3;++f)map.setIconFrame(id,s,f,blank);
        }
        selected=id;commit(p,undo,map,duplicate?"Copy location icon":"Create location icon");
    });};
    QObject::connect(add,&QPushButton::clicked,&dialog,[&]{create(false);});QObject::connect(copy,&QPushButton::clicked,&dialog,[&]{create(true);});
    QObject::connect(remove,&QPushButton::clicked,&dialog,[&]{guarded([&]{auto map=*p->campaignMap();map.removeIcon(selected);selected=0;commit(p,undo,map,"Delete location icon");});});
    QObject::connect(import,&QPushButton::clicked,&dialog,[&]{guarded([&]{
        const auto path=QFileDialog::getOpenFileName(&dialog,"Replace icon frame",{},"Images (*.png *.bmp *.jpg *.jpeg)");if(path.isEmpty())return;
        const int s=state->currentIndex()+1,f=frame->currentIndex();const auto size=p->campaignMap()->iconFrame(selected,s,f).size();
        ArtImageAssignment options{size,"campaignIcon","Replace icon frame","16 colours · transparent or opaque pixels",false,true,true};
        const auto prepared=prepareArtworkImage(&dialog,imageToSrgb(loadArtworkImage(path)),options,[&](const QImage &image,int threshold,bool dither){
            auto source=image;for(int y=0;y<source.height();++y)for(int x=0;x<source.width();++x){auto c=source.pixelColor(x,y);c.setAlpha(c.alpha()>=threshold?255:0);source.setPixelColor(x,y,c);}
            const auto data=CampaignMap::prepareArtwork(source,16,false,dither);return ArtImageMapping{data,CampaignMap::artworkImage(data,size,16),{}};
        });
        if(prepared){auto map=*p->campaignMap();auto data=*prepared;data.resize(160,0);map.setIconFrame(selected,s,f,data);commit(p,undo,map,"Replace location icon frame");}
    });});
    QObject::connect(exportPng,&QPushButton::clicked,&dialog,[&]{guarded([&]{const auto path=QFileDialog::getSaveFileName(&dialog,"Export icon frame",p->campaignMap()->iconName(selected)+".png","PNG (*.png)");if(!path.isEmpty()&&!p->campaignMap()->iconFrame(selected,state->currentIndex()+1,frame->currentIndex()).save(path))throw Error("Could not save icon image");});});
    QObject::connect(undoButton,&QPushButton::clicked,undo,&QUndoStack::undo);QObject::connect(redoButton,&QPushButton::clicked,undo,&QUndoStack::redo);
    QObject::connect(undo,&QUndoStack::indexChanged,&dialog,refresh);refresh();dialog.exec();
}
}
