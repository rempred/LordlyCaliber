#include "engine/campaign_map.h"
#include "engine/image_colors.h"
#include "engine/artwork_dither.h"
#include <algorithm>
#include <limits>
namespace lordly {
namespace {
void frameId(int state,int frame) {
    if(state<1 || state>2 || frame<0 || frame>2)throw Error("Choose an icon appearance and frame");
}
void nameCheck(const QString &name) {
    if(name.trimmed().isEmpty() || name.size()>80)throw Error("Use 1–80 characters for an icon name");
}
int channel(quint16 word,int axis) {return (word>>(axis==0?11:axis==1?6:1))&31;}
}
// Indexed image conversion uses the same weighted median-cut approach as portraits.
// A transparent entry is reserved only when the input actually needs one.
QByteArray CampaignMap::prepareArtwork(const QImage &input,int colors,bool opaque,bool dither) {
    if(input.isNull() || input.width()>512 || input.height()>320 || (colors!=16 && colors!=256))
        throw Error("Invalid world-map artwork dimensions");
    const auto image=imageToSrgb(input);QMap<quint16,int> histogram;bool transparent=!opaque;
    QVector<quint16> words;words.reserve(image.width()*image.height());
    for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x) {
        const auto c=image.pixelColor(x,y);
        if(opaque && c.alpha()!=255)throw Error("The map background must be fully opaque");
        quint16 word=0;
        if(c.alpha()>=128)word=quint16(((c.red()*31+127)/255<<11)|((c.green()*31+127)/255<<6)|((c.blue()*31+127)/255<<1)|1);
        if(word)++histogram[word];else transparent=true;
        words<<word;
    }
    struct Color {quint16 word;int count;};using Box=QVector<Color>;
    Box all;for(auto it=histogram.begin();it!=histogram.end();++it)all.append({it.key(),it.value()});
    QVector<Box> boxes;if(!all.isEmpty())boxes<<all;
    while(boxes.size()<colors-int(transparent)) {
        int selected=-1,axis=0;quint64 best=0;
        for(int n=0;n<boxes.size();++n)if(boxes[n].size()>1)for(int a=0;a<3;++a) {
            quint64 weight=0,sum=0,square=0;
            for(const auto &c:boxes[n]){const quint64 v=channel(c.word,a);weight+=c.count;sum+=v*c.count;square+=v*v*c.count;}
            const auto score=(square*weight-sum*sum)*(a==0?77:a==1?150:29)/weight;
            if(score>best){best=score;selected=n;axis=a;}
        }
        if(selected<0)break;
        auto box=boxes[selected];std::sort(box.begin(),box.end(),[&](const Color &a,const Color &b){
            const auto x=channel(a.word,axis),y=channel(b.word,axis);return x!=y?x<y:a.word<b.word;
        });
        int total=0,sum=0,split=0;for(const auto &c:box)total+=c.count;
        do{sum+=box[split++].count;}while(split<box.size()-1 && sum*2<total);
        boxes[selected]=box.first(split);boxes<<box.sliced(split);
    }
    QVector<quint16> palette;if(transparent)palette<<0;
    for(const auto &box:boxes) {
        int total=0,sums[3]={};for(const auto &c:box){total+=c.count;for(int a=0;a<3;++a)sums[a]+=channel(c.word,a)*c.count;}
        palette<<quint16(((sums[0]+total/2)/total<<11)|((sums[1]+total/2)/total<<6)|((sums[2]+total/2)/total<<1)|1);
    }
    QByteArray result(colors*2+(words.size()+(colors==16?1:0))/(colors==16?2:1),0);
    for(int i=0;i<palette.size();++i)put16(result,i*2,palette[i]);
    QMap<quint16,int> lookup;
    for(int i=0;i<words.size();++i) {
        const auto word=words[i];int chosen=0;
        if(word) {
            if(!dither && lookup.contains(word))chosen=lookup[word];
            else {
                double best=std::numeric_limits<double>::max();
                for(int n=int(transparent);n<palette.size();++n) {
                    double distance=0;for(int a=0;a<3;++a){const auto delta=channel(word,a)+(dither?orderedColorOffset(i%image.width(),i/image.width()):0)-channel(palette[n],a);distance+=delta*delta*(a==0?77:a==1?150:29);}
                    if(distance<best){best=distance;chosen=n;}
                }
                lookup[word]=chosen;
            }
        }
        if(colors==256)result[512+i]=char(chosen);
        else result[32+i/2]|=char(chosen<<(i%2?0:4));
    }
    return result;
}
QImage CampaignMap::artworkImage(const QByteArray &bytes,QSize size,int colors) {
    if((colors!=16 && colors!=256) || size.isEmpty() || bytes.size()!=colors*2+(size.width()*size.height()+(colors==16?1:0))/(colors==16?2:1))
        throw Error("Invalid indexed world-map artwork");
    QImage image(size,QImage::Format_RGBA8888);
    for(int i=0;i<size.width()*size.height();++i) {
        const int index=colors==256?u8(bytes,512+i):(u8(bytes,32+i/2)>>(i%2?0:4))&15;
        const auto word=u16(bytes,index*2);
        image.setPixelColor(i%size.width(),i/size.width(),QColor((channel(word,0)*255+15)/31,(channel(word,1)*255+15)/31,(channel(word,2)*255+15)/31,(word&1)?255:0));
    }
    return image;
}
void CampaignMap::initializeArtwork() {
    for(int kind=0;kind<2;++kind) {
        IconArt art{kind?"Castle":"Flag",kind,{}};
        for(int state=1;state<=2;++state)for(int frame=0;frame<3;++frame) {
            const int palette=kind?(state==1?0x140:0x160):(state==1?0x100:0x120);
            const int pixels=kind?(state==1?0xf68:0xfe8):(state==1?0xcb0+frame*128:0xe30+frame*104);
            art.data+=markers_.mid(palette,32);
            art.data+=markers_.mid(pixels,kind||state==1?128:104);
            if(!kind && state==2)art.data+=QByteArray(24,0);
        }
        artworkIcons_[kind]=art;
    }
}
QString CampaignMap::iconName(int id)const {
    if(!artworkIcons_.contains(id))throw Error("Unknown map location icon");return artworkIcons_[id].name;
}
int CampaignMap::iconKind(int id)const {iconName(id);return artworkIcons_[id].kind;}
QByteArray CampaignMap::iconFrameBytes(int id,int state,int frame)const {
    iconName(id);frameId(state,frame);return artworkIcons_[id].data.mid(((state-1)*3+frame)*160,160);
}
QImage CampaignMap::iconFrame(int id,int state,int frame)const {
    const auto image=artworkImage(iconFrameBytes(id,state,frame),{16,16},16);
    return !iconKind(id)&&state==2?image.copy(0,0,16,13):image;
}
void CampaignMap::setIconFrame(int id,int state,int frame,const QByteArray &data) {
    iconName(id);frameId(state,frame);if(data.size()!=160)throw Error("Location icon frames need 16 × 16 pixels and 16 colours");
    artworkIcons_[id].data.replace(((state-1)*3+frame)*160,160,data);
    if(iconKind(id))for(int n=0;n<3;++n)artworkIcons_[id].data.replace(((state-1)*3+n)*160,160,data);
}
void CampaignMap::renameIcon(int id,const QString &name){iconName(id);nameCheck(name);artworkIcons_[id].name=name.trimmed();}
int CampaignMap::createIcon(const QString &name,int source) {
    iconName(source);nameCheck(name);
    for(int id=2;id<iconCapacity;++id)if(!artworkIcons_.contains(id)){artworkIcons_[id]=artworkIcons_[source];artworkIcons_[id].name=name.trimmed();return id;}
    throw Error("All 64 location icons are in use");
}
void CampaignMap::removeIcon(int id) {
    iconName(id);if(id<2)throw Error("Original icons cannot be deleted");
    for(int n=1;n<=nodeCount;++n)if(icon(n)==id)throw Error("Choose another icon for locations using this one before deleting it");
    artworkIcons_.remove(id);
}
QJsonObject CampaignMap::iconCatalog()const {
    QJsonObject out;for(auto it=artworkIcons_.begin();it!=artworkIcons_.end();++it)
        out[QString::number(it.key())]=QJsonObject{{"name",it->name},{"kind",it->kind},{"frames",QString::fromLatin1(it->data.toBase64())}};
    return out;
}
void CampaignMap::loadIconCatalog(const QJsonObject &catalog) {
    QMap<int,IconArt> parsed;
    for(auto it=catalog.begin();it!=catalog.end();++it) {
        bool ok;const int id=it.key().toInt(&ok);const auto o=it->toObject();
        const auto encoded=o["frames"].toString().toLatin1();const auto data=QByteArray::fromBase64(encoded);
        if(!ok || QString::number(id)!=it.key() || id<0 || id>=iconCapacity || o.size()!=3 || !o["name"].isString() ||
           (o["kind"]!=0 && o["kind"]!=1) || data.size()!=960 || data.toBase64()!=encoded)
            throw Error("Invalid location icon catalog");
        nameCheck(o["name"].toString());parsed[id]={o["name"].toString(),o["kind"].toInt(),data};
    }
    if(!parsed.contains(0)||!parsed.contains(1)||parsed[0].kind!=0||parsed[1].kind!=1)throw Error("Missing original location icons");
    artworkIcons_=std::move(parsed);
}
void CampaignMap::setBackground(const QByteArray &data) {
    if(data.size()!=512+mapWidth*mapHeight)throw Error("Map artwork must be 512 × 320 pixels");
    for(int i=512;i<data.size();++i)if(!(u16(data,u8(data,i)*2)&1))throw Error("Map artwork must be opaque");
    mapArtwork_=data;
}
QJsonObject CampaignMap::artworkEdits()const {
    QJsonObject out;
    if(mapArtwork_!=originalMapArtwork_)out["background"]=QString::fromLatin1(mapArtwork_.toBase64());
    auto original=*this;original.artworkIcons_=originalArtworkIcons_;
    if(iconCatalog()!=original.iconCatalog())out["icons"]=iconCatalog();
    return out;
}
} // namespace lordly
