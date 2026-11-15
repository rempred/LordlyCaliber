#include "rebuild/scene_logic.h"
#include "engine/class_data.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
static void initSceneLogic(){Q_INIT_RESOURCE(scene_logic_sources);}
namespace lordly {
namespace {
constexpr int originalStart=0x284288,originalSize=8000;
constexpr int spriteSweepCall=0x2a0738;
constexpr quint32 spriteSweepWord=0x0c08c3f5;
QByteArray read(const QString &path){initSceneLogic();QFile f(path);if(!f.open(QIODevice::ReadOnly))throw Error("Missing scene story-logic source");return f.readAll();}
quint32 jumpTarget(const QByteArray &b){return (u32(b,originalStart)&0x3ffffffU)*4U|0x80000000U;}
QByteArray originalOwner(const Rom &rom){
    auto owner=rom.bytes().mid(originalStart,originalSize);
    if((u32(owner,0)>>26)==2U){
        const auto classes=ClassPlacement::read(rom);if(!classes)throw Error("Cutscene dispatcher changed without owned runtime data");
        const auto target=jumpTarget(rom.bytes());
        if(target<classes->layout.indexBase+32||target>=classes->layout.end)throw Error("Unknown cutscene dispatcher target");
        const auto at=classes->dataRomOffset+target-classes->layout.indexBase-32;
        const auto header=rom.bytes().mid(at,32);
        if(header.first(8)!="LCSCLOG1"||u32(header,8)!=1||u32(header,12)>classes->layout.end-target||
           crc32(rom.bytes().mid(at+32,u32(header,12)))!=u32(header,16))throw Error("Damaged cutscene story-logic runtime");
        if(u32(rom.bytes(),spriteSweepCall)!=(0x0c000000U|((u32(header,28)>>2)&0x3ffffffU)))throw Error("Cutscene sprite update hook changed");
        put32(owner,0,u32(header,20));put32(owner,4,u32(header,24));
    }
    const auto config=QJsonDocument::fromJson(read(":/game/director/story-logic.json")).object();
    if(sha256(owner).toUpper()!=config["original"].toObject()["sha256"].toString().toLatin1().toUpper())throw Error("Cutscene dispatcher contains unknown edits");
    return owner;
}
}
bool needsSceneLogic(const Project &p){
    if(!p.scenePrograms())return false;
    for(const auto &id:p.scenePrograms()->scenes())for(const auto &c:p.scenePrograms()->program(id).commands)
        if((c.opcode==0x8e&&QSet<quint32>{0x4c430000U,0x4c500000U,0x4c510000U,0x4c4d0000U,0x4c4e0000U,0x4c540000U}.contains(c.operands[0]&0xffff0000U))||
           (c.opcode==0x8f&&QSet<quint32>{0x4c430000U,0x4c4d0000U}.contains(c.operands[2]&0xffff0000U)))return true;
    return false;
}
bool hasSceneLogicRuntime(const Rom &rom){
    if(rom.revision()||(u32(rom.bytes(),originalStart)>>26)!=2U)return false;
    const auto classes=ClassPlacement::read(rom);if(!classes)return false;
    const auto target=jumpTarget(rom.bytes());
    if(target<classes->layout.indexBase+32||target>=classes->layout.end)return false;
    const auto at=classes->dataRomOffset+target-classes->layout.indexBase-32;
    if(rom.bytes().mid(at,8)!="LCSCLOG1")return false;
    originalOwner(rom);return true;
}
CompiledModule buildSceneLogicRuntime(const Compiler &compiler,const QString &dir,quint32 address,quint32 get,quint32 set){
    if(!get||!set)throw Error("Scene story flags require saved flag services");
    const auto config=QJsonDocument::fromJson(read(":/game/director/story-logic.json")).object();
    QMap<QString,quint32> symbols;const auto bindings=config["bindings"].toObject();
    for(auto i=bindings.begin();i!=bindings.end();++i)symbols[i.key()]=quint32(i->toInteger());
    symbols["lc_scene_flag_get"]=get;symbols["lc_scene_flag_set"]=set;
    symbols["lc_original_scene_poses"]=0x80230fd4U;
    symbols["lc_scene_sprite_motion"]=0x80300000U;
    auto module=compiler.compileModule(read(":/game/director/story-logic.c"),dir,"scene_story_logic",address+32,{},symbols,true);
    const auto length=module.bytes.size();const auto dataOffset=(length+15)&~15;
    symbols["lc_scene_sprite_motion"]=address+32+quint32(dataOffset);
    module=compiler.compileModule(read(":/game/director/story-logic.c"),dir,"scene_story_logic",address+32,{},symbols,true);
    if(module.bytes.size()!=length)throw Error("Scene runtime layout changed during relocation");
    if(module.entries.value("func_00284288")!=address+32)throw Error("Unexpected scene dispatcher layout");
    QByteArray header(32,0);header.replace(0,8,"LCSCLOG1");put32(header,8,1);put32(header,12,quint32(module.bytes.size()));put32(header,16,crc32(module.bytes));
    header.replace(20,8,QByteArray::fromHex(config["originalPrologue"].toString().toLatin1()));
    put32(header,28,module.entries.value("lc_scene_sprites_update"));
    // Original prologue is authenticated against the complete owner on install.
    // The accepted baseline is available in the source ROM at that boundary.
    module.bytes.append(QByteArray(dataOffset-length+30*40,0));
    module.bytes.prepend(header);module.address=address;return module;
}
void installSceneLogicRuntime(QByteArray &image,const Rom &source,quint32 entry){
    const auto owner=originalOwner(source);
    // Other consumers may change outside this owner; retain all of those edits.
    image.replace(originalStart,8,owner.first(8));
    if(!hasSceneLogicRuntime(source)&&u32(source.bytes(),spriteSweepCall)!=spriteSweepWord)throw Error("Scene pose scheduler contains unknown edits");
    put32(image,spriteSweepCall,spriteSweepWord);
    if(!entry)return;
    const auto classes=ClassPlacement::read(Rom::fromBytes(image));if(!classes)throw Error("Missing scene runtime allocation");
    if(entry<classes->layout.indexBase+32||entry>=classes->layout.end)throw Error("Scene runtime is outside permanent storage");
    const auto at=classes->dataRomOffset+entry-classes->layout.indexBase-32;
    if(image.mid(at,8)!="LCSCLOG1")throw Error("Scene runtime marker is missing");
    // Header prologue bytes must be set before the class allocation checksum is
    // sealed, so they are fixed from the independently accepted retail owner.
    if(image.mid(at+20,8)!=owner.first(8))throw Error("Scene runtime prologue differs from its accepted owner");
    const auto update=u32(image,at+28);
    if(update<entry||update>=entry+u32(image,at+12))throw Error("Invalid scene sprite update entry");
    put32(image,spriteSweepCall,0x0c000000U|((update>>2)&0x3ffffffU));
    put32(image,originalStart,0x08000000U|((entry>>2)&0x3ffffffU));put32(image,originalStart+4,0);
}
}
