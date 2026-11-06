#include "engine/battlefield_clone.h"
#include "engine/field_resources.h"
#include "engine/map_edits.h"
#include "engine/mission_tables.h"
#include "engine/promotion.h"
#include <QJsonArray>
#include <QSet>
#include <array>
#include <algorithm>

namespace lordly {
namespace {
constexpr quint32 base=0x594280;
constexpr std::array<quint32,5> roots{0x1e39dce,0x20712ea,0x2092506,0x2091e8c,0x18bd022};
constexpr quint32 originalModelAddress=0x8018f810;
constexpr int originalModelCount = 45, legacySlots = 4;
constexpr int modelSlots = legacySlots + MissionTables::keyCount - MissionTables::firstKey;
constexpr int bindingSize = 20 + modelSlots;
struct Description { quint32 table=0, count=45, runtime=0,priorOffset=0; std::array<quint32,5> keys=roots;
    std::array<int,modelSlots> targets{};std::array<quint32,5> priorKeys=roots; };
Description description(const Rom &rom) {
    const auto header=rom.bytes().mid(BattlefieldClone::descriptorOffset,64);
    if(!header.startsWith("LC08FLDS")){
        if(header!=QByteArray(64,char(-1)))throw Error("The battlefield directory descriptor belongs to another modification");
        return {};
    }
    const auto version = u32(header,8);
    if((version!=1 && version!=2)||u32(header,60)!=crc32(header.first(60)))throw Error("Custom battlefield directory metadata is damaged");
    Description out;out.table=u32(header,12);out.count=u32(header,16);out.runtime=u32(header,40);
    if(out.count<originalModelCount+1||out.count>originalModelCount+(version==1?legacySlots:modelSlots)||out.table<0x2800000||quint64(out.table)+out.count*12>quint64(rom.bytes().size()))
        throw Error("Custom battlefield model table exceeds its allocation");
    for(int i=0;i<5;++i){out.keys[i]=u32(header,20+i*4);if(out.keys[i]<0x2800000-base||quint64(out.keys[i])+base+4>quint64(rom.bytes().size()))throw Error("Custom battlefield resource directory is outside the cartridge");}
    if(crc32(rom.bytes().mid(out.table,out.count*12))!=u32(header,44))throw Error("Custom battlefield model table is damaged");
    const auto prior=u32(header,52);out.priorOffset=prior;
    const int size = version==1 ? 20 : bindingSize;
    if(prior<0x2800000||quint64(prior)+size>quint64(rom.bytes().size())||crc32(rom.bytes().mid(prior,size))!=u32(header,56))throw Error("Custom battlefield loader references are damaged");
    for(int i=0;i<5;++i)out.priorKeys[i]=u32(rom.bytes(),prior+i*4);
    QSet<int> seen;
    for (int i = 0; i < int(out.count) - originalModelCount; ++i) {
        const int key = version == 1 ? u8(header, 48 + i) : u8(rom.bytes(), prior + 20 + i);
        if (!MissionTables::authoringKey(key) || seen.contains(key))
            throw Error("Custom battlefield model ownership is invalid");
        out.targets[i] = key;
        seen.insert(key);
    }
    return out;
}
quint32 append(QByteArray &bytes,const QByteArray &payload) {
    const qsizetype at=(bytes.size()+15)&~qsizetype(15);
    const qsizetype end=(at+4+payload.size()+3)&~qsizetype(3);
    if(payload.isEmpty()||quint64(end)>0x4000000)throw Error("Custom battlefield exceeds cartridge space");
    bytes+=QByteArray(at-bytes.size()+4,0);put32(bytes,at,quint32(payload.size()));bytes+=payload;
    bytes+=QByteArray(end-bytes.size(),0);
    return quint32(at-base);
}
QByteArray binary(const QJsonObject &o,const QString &key,int maximum=4*1024*1024) {
    const auto text=o[key].toString().toLatin1();const auto bytes=QByteArray::fromBase64(text);
    if(bytes.isEmpty()||bytes.size()>maximum||bytes.toBase64()!=text)throw Error("Invalid battlefield snapshot "+key);
    return bytes;
}
void save(QJsonObject &o,const QString &key,const QByteArray &value){o[key]=QString::fromLatin1(value.toBase64());}
QByteArray collection(const Rom &rom,quint32 root){return fieldResource(rom,BattlefieldClone::resourceKey(rom,root));}
// Original field routines and the independent briefing loader use these literal
// resource keys. Their selected child/member index remains an ordinary byte.
struct RootUse {int root;quint32 high,low;quint32 originalHigh,originalLow;};
constexpr RootUse resourceUses[]{
    {0,0xed908,0xed924,0x3c0401e3,0x34849dce},{0,0xed934,0xed938,0x3c0401e3,0x34849dce},
    {0,0xedbf0,0xedbf4,0x3c0401e3,0x34849dce},{0,0xedc84,0xedc88,0x3c0401e3,0x34849dce},
    {0,0xf03ac,0xf03b0,0x3c0401e3,0x34849dce},
    {1,0xebfb8,0xebfbc,0x3c040207,0x348412ea},{1,0xebff0,0xebff4,0x3c040207,0x348412ea},
    {2,0xec134,0xec138,0x3c040209,0x34842506},{2,0xedce8,0xedcf8,0x3c040209,0x34842506},
    {2,0xf03cc,0xf03d0,0x3c040209,0x34842506},
    {3,0xebf48,0xebf4c,0x3c040209,0x34841e8c},
    {4,0x2b50e4,0x2b50e8,0x3c04018b,0x3484d022}};
// These are all model-row loads in the original field and scenario consumers.
// The high half stays 0x8019 so unrelated uses of that shared LUI stay valid.
constexpr quint32 modelLoads[]{0xebec0,0xebef8,0xed920,0xed950,0xedbd4,0xedc14,0xedc68,0xedca8,
    0xede20,0xede7c,0xeded0,0xedf30,0xedf84,0xedff4,0xee208,0xee334,0xee364,0xee3f8,
    0xee568,0xee828,0xee888,0xee950,0xeeb30,0xeebac,0xef1b0,0xef1fc,0xef284,0xef2b8,
    0xef2ec,0xef37c,0xef3b0,0xef444,0xef490,0xef4dc,0xefd58,0xf0180,0xf0274,0xf17dc,
    0xf1f80,0xf31e8,0xf34b8,0x10384c};
constexpr quint32 modelInstructions[]{
    0xc424f814,0xc420f814,0x8ca5f810,0x8ca5f810,0x8c42f818,0x8ca5f810,
    0x8c42f818,0x8ca5f810,0x8c22f818,0x8c23f818,0x8c22f818,0x8c23f818,
    0x8c22f818,0x8c42f818,0x8c22f818,0x8c22f818,0x8c23f818,0x8c42f818,
    0x8c42f818,0x8c42f818,0x8c42f818,0x8c22f818,0x8c22f818,0x8c23f818,
    0x8c42f818,0x8c42f818,0x8c42f818,0x8c42f818,0x8c23f818,0x8c22f818,
    0x8c22f818,0x8c22f818,0x8c22f818,0x8c42f818,0x8c22f818,0x8c42f818,
    0x8c22f818,0x8c42f818,0x8c63f818,0x8c22f818,0x8c22f818,0xc420f814};
static_assert(std::size(modelLoads)==std::size(modelInstructions));
void header(QByteArray &image,const Description &d){
    const bool expanded = d.count > originalModelCount + legacySlots ||
        std::any_of(d.targets.begin(), d.targets.end(), MissionTables::newKey);
    QByteArray h(64,0);h.replace(0,8,"LC08FLDS");put32(h,8,expanded ? 2 : 1);put32(h,12,d.table);put32(h,16,d.count);
    for(int i=0;i<5;++i)put32(h,20+i*4,d.keys[i]);put32(h,40,d.runtime);
    put32(h,44,crc32(image.mid(d.table,d.count*12)));
    if (!expanded)
        for (int i = 0; i < legacySlots; ++i) put8(h, 48 + i, d.targets[i]);
    QByteArray prior(expanded ? bindingSize : 20, 0);
    for (int i = 0; i < 5; ++i) put32(prior, i * 4, d.priorKeys[i]);
    if (expanded)
        for (int i = 0; i < modelSlots; ++i) put8(prior, 20 + i, d.targets[i]);
    quint32 priorOffset=d.priorOffset;
    if(!priorOffset||image.mid(priorOffset,prior.size())!=prior)priorOffset=append(image,prior)+base+4;
    put32(h,52,priorOffset);put32(h,56,crc32(prior));
    put32(h,60,crc32(h.first(60)));
    image.replace(BattlefieldClone::descriptorOffset,64,h);
}
}
quint32 BattlefieldClone::resourceKey(const Rom &rom,quint32 original){
    const auto d=description(rom);for(int i=0;i<5;++i)if(original==roots[i])return d.keys[i];return original;
}
QByteArray BattlefieldClone::modelTable(const Rom &rom){const auto d=description(rom);return d.table?rom.bytes().mid(d.table,d.count*12):QByteArray{};}
int BattlefieldClone::modelCount(const Rom &rom){return int(description(rom).count);}
void BattlefieldClone::replaceCollection(QByteArray &image,quint32 original,const QByteArray &payload){
    auto d=description(Rom::fromBytes(image));if(!d.table)throw Error("A private battlefield collection needs its ownership metadata");
    int index=-1;for(int i=0;i<5;++i)if(original==roots[i]||original==d.keys[i])index=i;
    if(index<0)throw Error("Unknown private battlefield collection");
    const auto codeRoot=(u32(image,resourceUses[0].high)&65535)<<16|(u32(image,resourceUses[0].low)&65535);
    if(codeRoot==d.keys[0])d.priorKeys=d.keys;
    d.keys[index]=append(image,payload);header(image,d);
}
QByteArray BattlefieldClone::modelRow(const Rom &rom,int model){const auto d=description(rom);if(model<0||quint32(model)>=d.count)throw Error("Battlefield model ID exceeds its native table");return rom.bytes().mid((d.table?d.table:0x65710)+model*12,12);}
bool BattlefieldClone::supportsSource(const Rom &rom,int source){
    if(source<1||source>64||source==54)return false;
    const int model=u8(MissionTables::row(rom,MissionTables::Scenario,source),3);
    return model>=5&&model<=41&&model!=23&&model!=24;
}
QJsonObject BattlefieldClone::capture(const Rom &rom,int source,bool blank,const QVector<int> &pointRows){
    if(!supportsSource(rom,source))throw Error("Choose an ordinary battlefield as the starting map");
    const auto row = MissionTables::row(rom, MissionTables::Scenario, source);
    const int model = u8(row, 3);
    if(model<5||model>41||model==23||model==24)throw Error("Choose an ordinary battlefield as the starting map");
    const auto media=modelRow(rom,model);const auto geometry=collection(rom,roots[0]),terrain=collection(rom,roots[1]);
    const auto positions=fieldMembers(collection(rom,roots[2])),bounds=fieldMembers(collection(rom,roots[3]));
    QJsonObject out{{"version",1},{"source",source},{"blank",blank}};
    save(out,"media",media);
    auto geometryData=fieldResource(rom,u32(geometry,u32(media,0)*4));
    auto terrainData=fieldResource(rom,u32(terrain,u8(row,5)*4));
    if(blank){
        auto maps=MapEdits::load(rom);const auto &s=maps.source(source);auto members=fieldMembers(geometryData);
        auto brush=s.field.terrainBrush(3,s.terrain[0].data,s.map.bounds);
        if(brush.texture.isNull())throw Error("Choose a starting battlefield that contains Plain terrain");
        members[0].data=blankFieldModel(s,brush,pointRows);members[1].data=blankFieldHeight(s);
        geometryData=encodeFieldMembers(members);auto grids=fieldMembers(terrainData);
        for(auto &grid:grids)if(grid.data.size()==65536||grid.data.size()==4096)grid.data.fill(char(3));
        terrainData=encodeFieldMembers(grids);
        auto flatMedia=media;put32(flatMedia,8,0x340c);save(out,"media",flatMedia);
    }
    save(out,"geometry",geometryData);save(out,"terrain",terrainData);
    auto points=positions[u8(row,7)].data;
    if(!pointRows.isEmpty()){
        const int count=int(u32(points,24));auto selected=points.first(28);put32(selected,24,pointRows.size());
        for(int i:pointRows){if(i<0||i>=count)throw Error("Custom battlefield base position is absent");selected+=points.mid(28+i*8,8);}points=selected;
    }
    save(out,"points",points);save(out,"bounds",bounds[u8(row,4)].data);
    const auto brief=collection(rom,roots[4]);const auto imageKey=u32(brief,(model-4)*4);
    if(blank){auto maps=MapEdits::load(rom);const auto &s=maps.source(source);
        const auto model=fieldMembers(geometryData).first().data;
        save(out,"briefing",fieldBriefing(s,Battlefield::load(rom,source,{},model),rom));
    }else save(out,"briefing",fieldResource(rom,imageKey));
    return out;
}
QByteArray BattlefieldClone::apply(const Rom &rom,int target,const QJsonObject &snapshot){
    if(!MissionTables::authoringKey(target)||snapshot["version"]!=1)throw Error("Invalid new battlefield target");
    auto image=rom.bytes();auto d=description(rom);
    MissionTables::ensure(image, target);
    const auto codeRoot=(u32(image,resourceUses[0].high)&65535)<<16|(u32(image,resourceUses[0].low)&65535);
    if(codeRoot==d.keys[0])d.priorKeys=d.keys;
    int model=-1;
    for (int i = 0; i < modelSlots; ++i)
        if (d.targets[i] == target) model = originalModelCount + i;
    const bool replace = model >= originalModelCount;
    if (!replace) model = int(d.count);
    if (model >= originalModelCount + modelSlots)
        throw Error("All independent battlefield slots are occupied");
    auto models=d.table?image.mid(d.table,d.count*12):image.mid(0x65710,45*12);
    std::array<QByteArray,5> data;for(int i=0;i<5;++i)data[i]=collection(rom,roots[i]);
    auto media=binary(snapshot,"media",12);if(media.size()!=12)throw Error("Invalid battlefield media record");
    const int geometryIndex=replace?int(u32(models,model*12)):data[0].size()/4;
    const int terrainIndex=replace?41+model-45:data[1].size()/4;
    auto points=fieldMembers(data[2]),bounds=fieldMembers(data[3]);
    const int pointIndex=replace?41+model-45:points.size();
    const int boundIndex=replace?41+model-45:bounds.size();
    if(geometryIndex>127||terrainIndex>127||pointIndex>127||boundIndex>127)throw Error("Battlefield resource selector exceeds the native signed byte range");
    const auto geometry=binary(snapshot,"geometry"),terrain=binary(snapshot,"terrain"),briefing=binary(snapshot,"briefing");
    const auto capturedPoints=binary(snapshot,"points"),capturedBounds=binary(snapshot,"bounds",32);
    auto selectors=[&]{
        auto row = MissionTables::row(Rom::fromBytes(image), MissionTables::Scenario, target);
        put8(row, 3, model);
        put8(row, 4, boundIndex);
        put8(row, 5, terrainIndex);
        put8(row, 7, pointIndex);
        MissionTables::replace(image, MissionTables::Scenario, target, row);
    };
    // Reopening an exported ROM replays its captured scenario descriptors. Reuse
    // authenticated private resources when their payloads have not changed.
    if(replace&&models.mid(model*12+4,8)==media.mid(4)&&
       fieldResource(rom,u32(data[0],geometryIndex*4))==geometry&&
       fieldResource(rom,u32(data[1],terrainIndex*4))==terrain&&
       fieldResource(rom,u32(data[4],(model-4)*4))==briefing&&
       points[pointIndex].data==capturedPoints&&bounds[boundIndex].data==capturedBounds){
        selectors();return image;
    }
    auto putKey=[&](int root,int index,const QByteArray &payload){if(index*4==data[root].size())data[root]+=QByteArray(4,0);if(index<0||index*4+4>data[root].size())throw Error("Invalid private battlefield resource index");put32(data[root],index*4,append(image,payload));};
    putKey(0,geometryIndex,geometry);putKey(1,terrainIndex,terrain);
    putKey(4,model-4,briefing);
    if(pointIndex==points.size())points.append({{},capturedPoints,0});else points[pointIndex]={{},capturedPoints,0};
    if(boundIndex==bounds.size())bounds.append({{},capturedBounds,0});else bounds[boundIndex]={{},capturedBounds,0};
    data[2]=encodeFieldMembers(points);data[3]=encodeFieldMembers(bounds);
    for(int i=0;i<5;++i)d.keys[i]=append(image,data[i]);
    put32(media,0,geometryIndex);if(replace)models.replace(model*12,12,media);else models+=media;
    d.count=models.size()/12;const auto tableKey=append(image,models);d.table=tableKey+base+4;d.targets[model-45]=target;
    selectors();
    header(image,d);return image;
}
void BattlefieldClone::validate(const Rom &rom,int target,const QJsonObject &snapshot){
    const auto row = MissionTables::row(rom, MissionTables::Scenario, target);
    const int model = u8(row, 3);
    const auto media = modelRow(rom,model);
    if(model<45||media.mid(4)!=binary(snapshot,"media",12).mid(4))throw Error("Custom battlefield media differs from its snapshot");
    const auto points = fieldMembers(collection(rom, roots[2]));
    const auto bounds = fieldMembers(collection(rom, roots[3]));
    const int point = u8(row, 7), bound = u8(row, 4);
    if(point >= points.size() || bound >= bounds.size() ||
       fieldResource(rom,fieldChild(rom,roots[0],u32(media,0)))!=binary(snapshot,"geometry")||
       fieldResource(rom,fieldChild(rom,roots[1],u8(row,5)))!=binary(snapshot,"terrain")||
       points[point].data!=binary(snapshot,"points")||
       bounds[bound].data!=binary(snapshot,"bounds",32) ||
       fieldResource(rom, fieldChild(rom, roots[4], model - 4)) != binary(snapshot, "briefing"))
        throw Error("Custom battlefield resources differ from their snapshot");
}
void BattlefieldClone::linkRuntime(QByteArray &image,quint32 tableAddress){
    auto d=description(Rom::fromBytes(image));if(!d.table)return;
    if(tableAddress<0x80188000||quint64(tableAddress)+d.count*12>0x8018aa70||tableAddress%16)
        throw Error("Extended battlefield model table exceeds its resident allocation");
    for(const auto &use:resourceUses){
        const auto hi=u32(image,use.high),lo=u32(image,use.low);
        if((hi&0xffff0000)!=(use.originalHigh&0xffff0000)||(lo&0xffff0000)!=(use.originalLow&0xffff0000))throw Error("Battlefield resource loader differs from its supported instructions");
        const auto previous=(hi&65535)<<16|(lo&65535);
        if(previous!=roots[use.root]&&previous!=d.keys[use.root]&&previous!=d.priorKeys[use.root])throw Error("Battlefield resource loader has an unrelated redirect");
        put32(image,use.high,(hi&0xffff0000)|(d.keys[use.root]>>16));put32(image,use.low,(lo&0xffff0000)|(d.keys[use.root]&65535));
    }
    for(size_t i=0;i<std::size(modelLoads);++i){const auto at=modelLoads[i],instruction=u32(image,at);const int low=qint16(instruction&65535);
        if((instruction&0xffff0000)!=(modelInstructions[i]&0xffff0000))throw Error("Battlefield model consumer differs from its supported instruction");
        int delta=low-qint16(originalModelAddress&65535);
        if(delta<0||delta>8||delta%4){delta=low-qint16(d.runtime&65535);if(!d.runtime||delta<0||delta>8||delta%4)throw Error("Battlefield model consumer differs from its supported table");}
        put32(image,at,(instruction&0xffff0000)|((tableAddress+delta)&65535));
    }
    d.runtime=tableAddress;header(image,d);
}
}
