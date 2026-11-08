#include "rebuild/new_campaign_runtime.h"
#include "engine/new_campaign.h"
#include <QJsonArray>
namespace lordly {
namespace {
constexpr quint32 dispatch=0x66658,noop=0x80171b58;
const QByteArray marker("LCSTART1",8);
}
NewCampaignInput prepareNewCampaign(const Project &project) {
    NewCampaignInput result;
    if(!project.sceneEvents())return result;
    result.settings=NewCampaign::settings(*project.sceneEvents());
    NewCampaign::validate(result.settings);
    if(!result.settings.isEmpty()) {
        const auto map=project.campaignMap()?*project.campaignMap():CampaignMap(project.rom());
        result.location=map.position(result.settings["location"].toInt());
    }
    for(const auto &v:result.settings["armies"].toArray()) {
        const int id=v.toObject()["id"].toInt();
        result.armies[id]=NewCampaign::armyMemory(project,NewCampaign::armyBytes(result.settings,id));
    }
    return result;
}
CompiledModule buildNewCampaignRuntime(const NewCampaignInput &input,const Compiler &compiler,
    const QString &directory,quint32 address,const std::function<quint32(quint32)> &relocate,quint32 customFlags) {
    CompiledModule result;if(input.settings.isEmpty())return result;
    QByteArray data(16*4,0),config;
    for(auto i=input.armies.begin();i!=input.armies.end();++i) {
        put32(data,i.key()*4,address+data.size());data+=i.value();
    }
    while(data.size()%16)data.append(char(0));
    config+="#define LC_SPANS {";int offset=0;
    for(auto [at,size]:NewCampaign::armySpans()) {
        if(offset)config+=',';
        config+="{"+QByteArray::number(relocate(at))+"U,"+QByteArray::number(size)+","+QByteArray::number(offset)+"}";
        offset+=size;
    }
    config+="}\n#define LC_LOCATIONS {";
    for(int id=1;id<=40;++id) {if(id>1)config+=',';config+=input.settings["locations"].toArray().contains(id)?'1':'0';}
    config+="}\n#define LC_START "+QByteArray::number(input.settings["location"].toInt())+
        "\n#define LC_GOTH "+QByteArray::number(input.settings["goth"].toInt())+
        "U\n#define LC_CHAOS "+QByteArray::number(input.settings["chaos"].toInt())+
        "\n#define LC_PLAYER_ROSTER "+QByteArray::number(relocate(0x80193bc0))+"U"+
        "\n#define LC_CUSTOM_FLAGS "+QByteArray::number(customFlags)+"U\n";
    const int x=qBound(0,input.location.x()-128,256),y=qBound(0,input.location.y()-96,124);
    config+="#define LC_SCROLL_X "+QByteArray::number(x)+"\n#define LC_SCROLL_Y "+QByteArray::number(y)+
        "\n#define LC_CURSOR_X "+QByteArray::number(input.location.x()-x+32)+
        "\n#define LC_CURSOR_Y "+QByteArray::number(input.location.y()-y+24)+"\n";
    const QByteArray source=R"C(
#include "new_campaign_config.h"
typedef unsigned char u8;
typedef unsigned int u32;
extern const u8 *const lc_starting_armies[16];
extern void lc_expand_saved_name(const u8 *packed,u8 *name);
struct Span {u32 address,size,offset;};
static const struct Span spans[]=LC_SPANS;
static const u8 locations[40]=LC_LOCATIONS;
static void clear(u32 address,u32 size) {u8 *p=(u8 *)address;while(size--)*p++=0;}
void lc_campaign_action(unsigned short *registers,u32 operation) {
    u32 n,i,id;const u8 *army;u8 *to;
    /* Only New Game may replace the whole army. A stray authored instruction
       in a mission or a loaded save cannot erase an existing player's roster. */
    if(*(volatile u8 *)0x801936a8!=1U)return;
    if(operation==0U) {
        id=registers[0];if(id>=16U)return;army=lc_starting_armies[id];if(!army)return;
        for(n=0;n<sizeof(spans)/sizeof(spans[0]);++n) {
            to=(u8 *)spans[n].address;
            for(i=0;i<spans[n].size;++i)to[i]=army[spans[n].offset+i];
        }
        /* The army stores names in the battery save format. Use the native
           save loader's expansion before exposing the roster to live menus. */
        for(n=1;n<100;++n) {
            u8 packed[14];
            to=(u8 *)LC_PLAYER_ROSTER+n*56;
            if(!to[17])continue;
            for(i=0;i<13;++i)packed[i]=to[i];
            packed[13]=255;
            for(i=0;i<17;++i)to[i]=0;
            lc_expand_saved_name(packed,to);
        }
    } else if(operation==1U) {
        clear(0x80193686U,32); /* persistent retail story flags */
        if(LC_CUSTOM_FLAGS)clear(LC_CUSTOM_FLAGS,64);
        clear(0x80196a81U,5); /* retail road unlock bits */
        clear(0x80196a86U,13); /* 52 two-bit location states, including chapter selectors */
        clear(0x80196a93U,6); /* mission markers */
        *(u32 *)0x80196a6cU=LC_GOTH;*(u8 *)0x801936a9U=LC_CHAOS;
        for(i=0;i<40;++i)if(locations[i]) {
            to=(u8 *)0x80196a86U;
            to[i>>2]|=(u8)(1U<<((i&3U)*2U));
        }
    } else if(operation==2U) {
        *(u8 *)0x80196a9aU=0x80U|LC_START;
        *(u8 *)0x801936aaU=LC_START;
        *(unsigned short *)0x80197b08U=0;
        *(unsigned short *)0x80196a9cU=LC_SCROLL_X;
        *(unsigned short *)0x80196a9eU=LC_SCROLL_Y;
        *(unsigned short *)0x80196aa0U=LC_CURSOR_X;
        *(unsigned short *)0x80196aa2U=LC_CURSOR_Y;
    }
}
)C";
    result=compiler.compileModule(source,directory,"new_campaign",address+data.size(),
        {{"new_campaign_config.h",config}},{{"lc_starting_armies",address},
            {"lc_expand_saved_name",0x8017aaf4}},true);
    result.address=address;result.bytes.prepend(data);
    while(result.bytes.size()%16)result.bytes.append(char(0));
    result.bytes+=marker;
    const quint32 bridge=address+result.bytes.size();
    result.bytes+=compiler.assembleOwner(R"ASM(
.set noreorder
.set noat
.globl lc_campaign_bridge
lc_campaign_bridge:
addiu $sp,$sp,-32
sw $ra,28($sp)
addiu $a0,$sp,112
lbu $a1,129($sp)
jal lc_campaign_action
nop
lw $ra,28($sp)
addiu $sp,$sp,32
j lc_campaign_increment
nop
)ASM",directory,"new_campaign_bridge",bridge,
        {{"lc_campaign_action",result.entries.value("lc_campaign_action")},{"lc_campaign_increment",noop}});
    while(result.bytes.size()%16)result.bytes.append(char(0));
    result.entries["lc_campaign_bridge"]=bridge;
    return result;
}
void linkNewCampaignRuntime(QByteArray &image,quint32 bridge,const Rom &source) {
    const quint32 old=u32(source.bytes(),dispatch);
    if(old!=noop) {
        const auto placement=ClassPlacement::read(source);
        if(!placement||old<placement->layout.indexBase+8||old>=placement->layout.end||
           source.bytes().mid(placement->dataRomOffset+old-placement->layout.indexBase-8,8)!=marker)
            throw Error("The New Campaign event instruction is occupied by another modification");
    }
    if(u32(image,dispatch)!=old)throw Error("Another feature changed the New Campaign event instruction");
    put32(image,dispatch,bridge?bridge:noop);
}
}
