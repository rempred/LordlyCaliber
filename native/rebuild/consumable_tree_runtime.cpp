#include "rebuild/consumable_tree_runtime.h"
#include "engine/consumable_tree.h"
#include "engine/consumable_visual.h"
#include "engine/resource.h"
#include "engine/capacity_placement.h"
#include <QFile>
#include <QJsonDocument>
namespace lordly {
namespace {
QByteArray read(const QString &name){QFile f(":/game/"+name);if(!f.open(QIODevice::ReadOnly))throw Error("Missing item tree source: "+name);return f.readAll();}
void align(QByteArray &b){while(b.size()%16)b.append(char(0));}
}
ConsumableTreePlacement buildConsumableTrees(const ConsumableTreeInput &input,quint32 address,quint32 offset,const Compiler &compiler,const QString &directory,const std::function<quint32(quint32)> &relocate){
    ConsumableTreePlacement p;p.address=address;p.offset=offset;p.model=input.effects.json();
    const auto capacity=CapacityPlacement::read(input.source);
    const auto sourceRom=capacity?capacity->canonical(input.source):input.source;
    auto &data=p.data;data=QByteArray(256,0);data.replace(0,8,"LCITEMD1");
    for(auto it=input.effects.trees().begin();it!=input.effects.trees().end();++it){
        put32(data,32+it.key().toInt()*4,address+data.size());data+=ConsumableTree::compile(it->toArray());
    }
    align(data);const auto json=QJsonDocument(p.model).toJson(QJsonDocument::Compact);put32(data,8,data.size());put32(data,12,json.size());data+=json;align(data);
    const auto params=address+data.size();
    for(const auto &v:ConsumableEffects::definitions()) {const auto d=v.toObject(),m=input.effects.models()[d["key"].toString()].toObject();const int at=data.size();data+=QByteArray(d["kind"]=="magnitude"?2:4,0);put16(data,at,quint16(m[d["kind"]=="magnitude"?"magnitude":"minimum"].toInt()));if(d["kind"]!="magnitude")put16(data,at+2,m["maximum"].toInt()-m["minimum"].toInt()+1);}
    align(data);
    const auto visualIndex = address + data.size(); const auto visualIndexOffset = data.size();
    data += QByteArray(45 * 4, 0); align(data);
    for (auto it = input.effects.visuals().begin(); it != input.effects.visuals().end(); ++it) {
        const auto visual = ConsumableVisual::fromJson(it->toObject());
        put32(data, visualIndexOffset + it.key().toInt() * 4, address + data.size());
        data += visual.pixels; data += visual.palette;
    }
    const auto nativePixels = address + data.size(); data += ConsumableVisual::retailPixels(input.source);
    const auto nativePalettes = address + data.size(); data += ConsumableVisual::retailPalettes(input.source);
    align(data);const auto state=address+data.size();data+=QByteArray(32768,0);
    QMap<QString,QByteArray> headers;
    headers["common/types.h"]=read("accepted/library/include/common/types.h");
    headers["game/scenario_item_views.h"]=read("accepted/library/include/game/scenario_item_views.h");
    QMap<QString,quint32> symbols;const auto base=ConsumableEffects::runtimeOwner()["symbols"].toObject();
    for(auto it=base.begin();it!=base.end();++it)symbols[it.key()]=relocate?relocate(quint32(it->toInteger())):quint32(it->toInteger());
    symbols["lc_effect_parameters"]=params;symbols["lc_tree_index"]=address+32;symbols["lc_tree_state"]=state;symbols["func_00040FF4"]=0x8016b0f4;
    symbols["lc_item_visual_index"] = visualIndex;
    symbols["lc_item_native_pixels"] = nativePixels;
    symbols["lc_item_native_palettes"] = nativePalettes;
    symbols["D_80214A6C"] = 0x80214a6c;
    symbols["D_80214EAC"] = 0x80214eac;
    symbols["osWritebackDCache"] = 0x80090140;
    symbols["func_000453e0"]=0x8016f4e0;symbols["func_00045440"]=0x8016f540;
    auto source=read("consumables/effects.c");source.replace("func_00041098(","lc_base_effect(");
    const auto native=compiler.compileModule(source,directory,"item_tree_base",address+data.size(),headers,symbols,true);data+=native.bytes;align(data);symbols["lc_base_effect"]=native.entries.value("lc_base_effect");
    const auto runtime=compiler.compileModule(read("consumables/tree.c"),directory,"item_tree_execute",address+data.size(),headers,symbols,true);data+=runtime.bytes;align(data);
    for(auto it=runtime.entries.begin();it!=runtime.entries.end();++it)symbols[it.key()]=it.value();
    put32(data, 24, symbols.value("lc_item_visual_select"));
    put32(data, 28, visualIndex);
    struct Hook{quint32 at,size,target;};QVector<Hook> hooks{{0x41098,1380,symbols["lc_tree_effect"]}};
    const auto owners=QJsonDocument::fromJson(read("consumables/tree-owners.json")).object()["owners"].toArray();
    for(const auto &v:owners){
        const auto owner=v.toObject();auto bindings=symbols;const auto original=owner["bindings"].toObject();
        for(auto it=original.begin();it!=original.end();++it)if(!bindings.contains(it.key()))bindings[it.key()]=relocate?relocate(quint32(it->toInteger())):quint32(it->toInteger());
        QMap<QString,QByteArray> inc=headers;
        for(const auto &h:owner["headers"].toArray()){const auto hdr=h.toObject();auto name=hdr["path"].toString();if(name.startsWith("include/"))name.remove(0,8);auto file=hdr["file"].toString();if(file.startsWith("game/"))file.remove(0,5);const auto content=read(file);if(sha256(content).toLower()!=hdr["sha256"].toString().toLatin1().toLower())throw Error("Accepted item header changed");inc[name]=content;}
        const auto text=read(owner["source"].toString());if(sha256(text)!=owner["sourceSha256"].toString().toLatin1())throw Error("Item tree source changed without regeneration");
        const auto part=owner["parts"].toArray()[0].toObject();const auto at=quint32(part["start"].toInteger()),size=quint32(part["size"].toInteger());
        for(const auto &pv:owner["parts"].toArray()){const auto part=pv.toObject();if(sha256(sourceRom.bytes().mid(part["start"].toInteger(),part["size"].toInt())).toLower()!=part["sha256"].toString().toLatin1().toLower())throw Error("An item owner or its table has unrecognized changes");}
        if(sha256(sourceRom.bytes().mid(at,size)).toLower()!=part["sha256"].toString().toLatin1().toLower())throw Error("Cannot replace an unrecognized item menu owner at "+QString::number(at,16));
        const auto code=compiler.compileModule(text,directory,owner["name"].toString()+"_item_tree",address+data.size(),inc,bindings,true);data+=code.bytes;align(data);
        const auto entry=code.entries.value(owner["name"].toString());if(!entry)throw Error("Missing compiled item menu entry");hooks.append({at,size,entry});
    }
    p.directory=data.size();p.count=hooks.size();put32(data,16,p.directory);put32(data,20,p.count);
    for(const auto &h:hooks){const auto at=data.size();data+=QByteArray(28,0);put32(data,at,h.at);put32(data,at+4,h.size);put32(data,at+8,h.target);data.replace(at+12,8,sourceRom.bytes().mid(h.at,8));put32(data,at+20,crc32(sourceRom.bytes().mid(h.at,h.size)));}
    align(data);p.size=data.size();if(p.size>0x40000||quint64(address)+p.size>0x80400000)throw Error("Item effect trees exceed their allocated memory");return p;
}
QByteArray installConsumableTrees(const QByteArray &source,const ConsumableTreePlacement &p){
    auto image=source;const auto old=image.mid(p.descriptorOffset,p.descriptorSize);
    const auto rom=Rom::fromBytes(source);const auto capacity=CapacityPlacement::read(rom);const auto canonical=capacity?capacity->canonical(rom).bytes():source;
    if(old!=QByteArray(p.descriptorSize,char(-1)))throw Error("Item tree descriptor overlaps unrelated data");
    for(quint32 i=0;i<p.count;i++) {const auto row=p.directory+i*28,at=u32(p.data,row),extent=u32(p.data,row+4),target=u32(p.data,row+8);if(crc32(canonical.mid(at,extent))!=u32(p.data,row+20))throw Error("Another exporter changed an item tree owner");put32(image,at,0x08000000U|((target>>2)&0x3ffffff));put32(image,at+4,0);}
    const auto d=p.descriptor();image.replace(p.descriptorOffset,d.size(),d);return image;
}
}
