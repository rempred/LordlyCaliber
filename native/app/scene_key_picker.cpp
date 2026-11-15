#include "app/scene_key_picker.h"
#include "engine/pose_preview.h"
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QSignalBlocker>
#include <type_traits>
namespace lordly {
namespace {
QImage frameImage(const Rom &rom,const ActorArt &art,int frame,int variant,const QByteArray &material,const ActorArtworkEdits &edits) {
    if(frame<0)return {};
    const auto layers=posePreviewLayers(art.frame(rom,frame,variant,0,{},edits.value(art.descriptorKey())),0,0,material);
    QRectF bounds;for(const auto &layer:layers)bounds|=layer.bounds();
    if(bounds.isEmpty())return {};
    auto image=paintPosePreview(layers,bounds.toAlignedRect());
    for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)if(qAlpha(image.pixel(x,y)))return image;
    return {};
}
template<class Player> QImage visibleFrame(const Rom &rom,const ActorArt &art,Player player,int variant,const ActorArtworkEdits &edits){
    QSet<QString> visited;QSet<QByteArray> drawn;
    for(int tick=0;tick<16384;++tick){
        if(player.advance()!=LORDLY_POSE_OK)throw Error(player.error());
        const auto &pose=[&]() -> const auto & {if constexpr(std::is_same_v<Player,ActorPosePlayer>)return player.actor();else return player.sprite();}();
        const auto opacity=[&]{if constexpr(std::is_same_v<Player,ActorPosePlayer>)return QByteArray(reinterpret_cast<const char *>(pose.material),16);else return QByteArray(reinterpret_cast<const char *>(pose.color),16);}();
        const auto step=QString("%1/%2/%3/%4").arg(pose.state).arg(pose.cursor).arg(pose.delay).arg(QString::fromLatin1(opacity.toHex()));
        if(visited.contains(step))break;visited.insert(step);
        const auto visual=QByteArray::number(pose.frame)+":"+opacity;
        if(!drawn.contains(visual)){drawn.insert(visual);auto image=frameImage(rom,art,pose.frame,variant,opacity,edits);if(!image.isNull())return image;}
        if(player.result()==0)break;
    }
    return {};
}
struct KeyFrames { int count; bool loops; };
template<class Player> KeyFrames countFrames(Player player) {
    QSet<int> frames;QSet<QString> visited;
    for(int tick=0;tick<16384;++tick){
        if(player.advance()!=LORDLY_POSE_OK)throw Error(player.error());
        const auto &pose=[&]() -> const auto & {if constexpr(std::is_same_v<Player,ActorPosePlayer>)return player.actor();else return player.sprite();}();
        const auto step=QString("%1/%2/%3").arg(pose.state).arg(pose.cursor).arg(pose.delay);
        if(visited.contains(step))return {int(frames.size()),true};
        visited.insert(step);if(pose.frame>=0)frames.insert(pose.frame);
        if(player.result()==0)return {int(frames.size()),false};
    }
    throw Error("Frame count requires more than 16384 animation updates");
}
}
SceneKeyPicker::SceneKeyPicker(const Rom &rom,QWidget *parent):QWidget(parent),rom_(rom) {
    setObjectName("sceneKeyPicker");auto layout=new QVBoxLayout(this);
    kind_=new QComboBox;kind_->setObjectName("sceneKeyKind");kind_->addItems({"Actor","Sprite"});
    banks_=new QComboBox;banks_->setObjectName("sceneKeyBank");
    for(int bank=1;bank<=ActorArt::bankCount(rom_);++bank)banks_->addItem(QString("Bank %1").arg(bank),bank);
    variants_=new QComboBox;variants_->setObjectName("sceneKeyAppearance");variants_->setIconSize({40,40});
    variants_->setToolTip("Artwork appearance used by the game within this bank. Applies to previews and placement.");
    layout->addWidget(kind_);layout->addWidget(banks_);layout->addWidget(variants_);
    keys_=new QListWidget;keys_->setObjectName("sceneKeyGrid");keys_->setViewMode(QListView::IconMode);
    keys_->setResizeMode(QListView::Adjust);keys_->setMovement(QListView::Static);
    keys_->setIconSize({64,64});keys_->setGridSize({126,140});keys_->setWordWrap(true);keys_->setSpacing(3);keys_->setTextElideMode(Qt::ElideNone);
    layout->addWidget(keys_,1);setMinimumWidth(240);
    audition_=new QWidget(this);audition_->setObjectName("sceneKeyAudition");
    auto auditionLayout=new QHBoxLayout(audition_);image_=new QLabel;image_->setObjectName("sceneKeyPreviewImage");image_->setAlignment(Qt::AlignCenter);
    image_->setMinimumSize(150,120);image_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Ignored);
    auditionLayout->addWidget(image_,1);auto controls=new QVBoxLayout;
    status_=new QLabel("Select an animation key to preview and place.");status_->setWordWrap(true);controls->addWidget(status_);
    play_=new QPushButton("Pause preview");play_->setCheckable(true);play_->setChecked(true);controls->addWidget(play_);
    loop_=new QCheckBox("Loop");loop_->setObjectName("sceneKeyLoop");controls->addWidget(loop_);
    auditionLayout->addLayout(controls,1);audition_->setMinimumHeight(135);
    timer_=new QTimer(this);timer_->setInterval(33);timer_->setTimerType(Qt::PreciseTimer);
    connect(timer_,&QTimer::timeout,this,&SceneKeyPicker::step);
    connect(play_,&QPushButton::toggled,this,[this](bool play){play_->setText(play?"Pause preview":"Play preview");if(play){if(finished_)restart();timer_->start();}else timer_->stop();});
    connect(banks_,&QComboBox::currentIndexChanged,this,&SceneKeyPicker::loadBank);
    connect(kind_,&QComboBox::currentIndexChanged,this,&SceneKeyPicker::loadBank);
    connect(variants_,&QComboBox::currentIndexChanged,this,&SceneKeyPicker::loadKeys);
    connect(keys_,&QListWidget::currentRowChanged,this,&SceneKeyPicker::selectKey);
    connect(keys_,&QListWidget::itemClicked,this,[this]{selectKey();});
    loadBank();
}
void SceneKeyPicker::setArtwork(const ActorArtworkEdits &a){if(artwork_==a)return;artwork_=a;loadBank();}
void SceneKeyPicker::hideEvent(QHideEvent *event){timer_->stop();play_->setChecked(false);QWidget::hideEvent(event);}
QVector<int> SceneKeyPicker::selectedKey()const{
    const auto item=keys_->currentItem();if(!item)return {};
    return {kind_->currentIndex(),banks_->currentData().toInt(),item->data(Qt::UserRole+1).toInt(),item->data(Qt::UserRole+2).toInt(),variants_->currentData().toInt()};
}
QImage SceneKeyPicker::firstFrame(int state,int variant)const{
    if(kind_->currentIndex()==1)return visibleFrame(rom_,*art_,SpritePosePlayer(rom_,*art_,state,variant),variant,artwork_);
    ActorPosePlayer player(rom_,*art_,state,variant);player.setLinkedObjectKnownAbsent(true);
    return visibleFrame(rom_,*art_,player,variant,artwork_);
}
void SceneKeyPicker::loadBank(){
    emit placementCancelled();
    timer_->stop();player_=std::monostate{};state_=-1;
    const QSignalBlocker guard(variants_);variants_->clear();
    try{
        art_=ActorArt::load(rom_,banks_->currentData().toInt());
        const auto selections=art_->selections();const auto count=art_->variantCount(rom_,artwork_.value(art_->descriptorKey()));int firstVisible=-1;
        for(int variant=0;variant<count;++variant){
            QImage thumbnail;
            for(const auto &key:selections){try{thumbnail=firstFrame(key.state,variant);}catch(const std::exception &){}if(!thumbnail.isNull())break;}
            variants_->addItem(QIcon(QPixmap::fromImage(thumbnail)),QString("Appearance %1%2").arg(variant).arg(thumbnail.isNull()?" · empty":""),variant);
            if(firstVisible<0&&!thumbnail.isNull())firstVisible=variant;
        }
        variants_->setCurrentIndex(qMax(0,firstVisible));loadKeys();
    }catch(const std::exception &e){art_.reset();keys_->clear();status_->setText(QString::fromUtf8(e.what()));}
}
void SceneKeyPicker::loadKeys(){
    emit placementCancelled();timer_->stop();player_=std::monostate{};state_=-1;image_->clear();
    const QSignalBlocker guard(keys_);keys_->clear();
    try{
        if(!art_)return;QMap<int,KeyFrames> counts;const int variant=variants_->currentData().toInt();
        for(const auto &key:art_->selections()){
            auto item=new QListWidgetItem(QString("Key %1\nFacing %2").arg(key.animation).arg(key.facing),keys_);
            item->setData(Qt::UserRole,key.state);item->setData(Qt::UserRole+1,key.animation);item->setData(Qt::UserRole+2,key.facing);
            try{const auto image=firstFrame(key.state,variant);if(!image.isNull())item->setIcon(QPixmap::fromImage(image.scaled(64,64,Qt::KeepAspectRatio,Qt::FastTransformation)));
                item->setData(Qt::UserRole+5,!image.isNull());
                if(!counts.contains(key.state)){
                    if(kind_->currentIndex()==1)counts[key.state]=countFrames(SpritePosePlayer(rom_,*art_,key.state));
                    else {ActorPosePlayer player(rom_,*art_,key.state);player.setLinkedObjectKnownAbsent(true);counts[key.state]=countFrames(player);}
                }
                const auto count=counts[key.state];item->setData(Qt::UserRole+3,count.count);item->setData(Qt::UserRole+4,count.loops);
                item->setText(item->text()+QString("\n%1 frame%2%3").arg(count.count).arg(count.count==1?"":"s").arg(count.loops?" · loop":""));
                item->setToolTip(QString("%1 distinct artwork frames reached by this animation, including linked states. %2").arg(count.count).arg(count.loops?"The animation loops.":"The animation finishes."));
                if(image.isNull()){item->setText(item->text()+"\nEmpty appearance");item->setToolTip(item->toolTip()+" No visible artwork in this appearance; select another appearance above.");}
            }
            catch(const std::exception &e){item->setText(item->text()+"\nCount unavailable");item->setToolTip(QString::fromUtf8(e.what()));}
        }
        status_->setText(QString("Bank %1 · %2 keys. Select one, then click in the scene.").arg(art_->bank()).arg(keys_->count()));
    }catch(const std::exception &e){art_.reset();status_->setText(QString::fromUtf8(e.what()));}
    image_->clear();
}
void SceneKeyPicker::selectKey(){
    if(!art_||!keys_->currentItem())return;
    const auto item=keys_->currentItem();state_=item->data(Qt::UserRole).toInt();
    {const QSignalBlocker guard(play_);play_->setChecked(true);play_->setText("Pause preview");}
    restart();
    emit keySelected(kind_->currentIndex()==1,banks_->currentData().toInt(),item->data(Qt::UserRole+1).toInt(),item->data(Qt::UserRole+2).toInt(),variants_->currentData().toInt());
}
void SceneKeyPicker::restart(){
    timer_->stop();ticks_=0;finished_=false;visited_.clear();try{
        const int variant=variants_->currentData().toInt();
        if(kind_->currentIndex()==1)player_.emplace<SpritePosePlayer>(rom_,*art_,state_,variant);
        else {player_.emplace<ActorPosePlayer>(rom_,*art_,state_,variant);std::get<ActorPosePlayer>(player_).setLinkedObjectKnownAbsent(true);}
        step();if(play_->isChecked())timer_->start();
    }catch(const std::exception &e){player_=std::monostate{};status_->setText(QString::fromUtf8(e.what()));}
}
void SceneKeyPicker::step(){
    try{
        if(finished_){if(loop_->isChecked()){restart();return;}play_->setChecked(false);return;}
        QString signature;int command=-1;
        if(auto p=std::get_if<ActorPosePlayer>(&player_)){p->advance();if(p->suspended())throw Error(p->error());}
        else if(auto sprite=std::get_if<SpritePosePlayer>(&player_)){sprite->advance();if(sprite->suspended())throw Error(sprite->error());}
        else return;
        if(auto p=std::get_if<ActorPosePlayer>(&player_)){const auto &a=p->actor();signature=QString("%1/%2/%3").arg(a.state).arg(a.cursor).arg(a.delay);command=p->query();}
        if(auto p=std::get_if<SpritePosePlayer>(&player_)){const auto &a=p->sprite();signature=QString("%1/%2/%3").arg(a.state).arg(a.cursor).arg(a.delay);command=p->query();}
        finished_=command==0||visited_.contains(signature);visited_.insert(signature);
        ++ticks_;draw();if(finished_&&!loop_->isChecked())play_->setChecked(false);
    }catch(const std::exception &e){timer_->stop();status_->setText(QString::fromUtf8(e.what()));}
}
void SceneKeyPicker::draw(){
    QImage image;
    const int variant=variants_->currentData().toInt();
    if(auto p=std::get_if<ActorPosePlayer>(&player_))image=frameImage(rom_,*art_,p->actor().frame,variant,QByteArray(reinterpret_cast<const char *>(p->actor().material),16),artwork_);
    if(auto p=std::get_if<SpritePosePlayer>(&player_))image=frameImage(rom_,*art_,p->sprite().frame,variant,QByteArray(reinterpret_cast<const char *>(p->sprite().color),16),artwork_);
    image_->setPixmap(image.isNull()?QPixmap{}:QPixmap::fromImage(image.scaled(image_->size(),Qt::KeepAspectRatio,Qt::FastTransformation)));
    const auto item=keys_->currentItem();if(item)status_->setText(QString("%1 · Bank %2 · key %3 · facing %4\nAppearance %5 · %6 s%7").arg(kind_->currentText()).arg(art_->bank()).arg(item->data(Qt::UserRole+1).toInt()).arg(item->data(Qt::UserRole+2).toInt()).arg(variant).arg(ticks_/30.,0,'f',2).arg(item->data(Qt::UserRole+5).toBool()?"":" · no visible artwork"));
}
}
