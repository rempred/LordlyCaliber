#include "engine/event_authoring.h"
#include <QSet>
#include <functional>
namespace lordly {
namespace {
int number(const QJsonObject &o,const QString &key,int low,int high,int fallback=0) {
    const auto v=o.value(key);const int n=v.isUndefined()?fallback:v.toInt(low-1);
    if((!v.isUndefined()&&(!v.isDouble()||v.toDouble()!=n))||n<low||n>high)
        throw Error("Invalid event value: "+key);
    return n;
}
void word(QByteArray &b,int op,int arg=0){b+=char(op);b+=char(arg);}
void load(QByteArray &b,int reg,int value){word(b,0x68+reg,value);word(b,0x88+reg,(value>>8)&255);}
int jump(QByteArray &b){const int at=b.size();word(b,0x68);word(b,2);return at;}
void link(QByteArray &b,int at,int target){
    if(target<0||target>65534||target%2)throw Error("Event jump exceeds its native pointer range");
    const auto delta=quint16((target-at-4)/2);b[at+1]=char(delta);b[at+3]=char(delta>>8);
}
struct Compiler {
    QByteArray bytes;int count=0,group=0;
    void budget(int depth){if(depth>16||++count>1024)throw Error("An event supports 1,024 steps and 16 nesting levels");}
    // Each predicate uses a short branch over a long jump. Tree size cannot
    // silently overflow an eight-bit conditional displacement.
    QPair<QVector<int>,QVector<int>> condition(const QJsonObject &c,int depth) {
        budget(depth);const auto type=c["type"].toString();
        if(type=="all"||type=="any") {
            const auto children=c["conditions"].toArray();if(children.isEmpty()||!c["conditions"].isArray())throw Error("Add a condition to AND / OR");
            QPair<QVector<int>,QVector<int>> out;
            for(int child=0;child<children.size();++child){const auto v=children[child];if(!v.isObject())throw Error("Invalid condition");
                auto next=condition(v.toObject(),depth+1);
                if(type=="all"){out.first+=next.first;out.second+=next.second;
                    if(child+1<children.size()){for(int at:out.first)link(bytes,at,bytes.size());out.first.clear();}}
                else {out.first+=next.first;out.second+=next.second;
                    if(child+1<children.size()){for(int at:out.second)link(bytes,at,bytes.size());out.second.clear();}}
            }return out;
        }
        if(type=="not") {if(!c["condition"].isObject())throw Error("NOT needs a condition");auto out=condition(c["condition"].toObject(),depth+1);return {out.second,out.first};}
        int relation=number(c,"comparison",10,15,10);
        if((type=="squad"&&(group!=3&&group!=4))||(type=="town"&&(group!=1&&group!=2))||(type=="squadState"&&(group<1||group>3)))throw Error("This condition needs a town visit or squad encounter WHEN");
        if(type=="flag") {load(bytes,2,number(c,"flag",0,767));word(bytes,0x61,0x62);load(bytes,2,number(c,"value",0,1));}
        else if(type=="chaos") {word(bytes,0x61,0xff);load(bytes,2,number(c,"value",0,100,50));}
        else if(type=="answer") {load(bytes,2,3);word(bytes,0x61,0x92);load(bytes,2,number(c,"value",0,255,255));}
        else if(type=="squad"||type=="town") {word(bytes,0x61,type=="town"?0xf8:0xfd);load(bytes,2,number(c,"value",type=="town"?1:0,255));}
        else if(type=="squadState") {word(bytes,0x67,0xfe);word(bytes,0x61,0x6f);load(bytes,2,129);word(bytes,0x91,2);load(bytes,2,number(c,"value",0,129,1));
            if(c["value"].toInt(1)!=0&&c["value"].toInt(1)!=1&&c["value"].toInt(1)!=128&&c["value"].toInt(1)!=129)throw Error("Unknown squad state");
            if(relation!=10&&relation!=11)throw Error("Choose is / is not for a squad state");}
        else throw Error("Unknown authored condition: "+type);
        word(bytes,(relation<<4)|6,2);const int no=jump(bytes),yes=jump(bytes);return {{yes},{no}};
    }
    void body(const QJsonArray &nodes,int depth=0) {
        for(const auto &v:nodes){budget(depth);if(!v.isObject())throw Error("Invalid event step");const auto n=v.toObject();const auto type=n["type"].toString();
            if(type=="if") {if(!n["condition"].isObject()||!n["then"].isArray()||!n["else"].isArray())throw Error("IF needs a condition, THEN and ELSE");
                auto branches=condition(n["condition"].toObject(),depth+1);for(int at:branches.first)link(bytes,at,bytes.size());
                body(n["then"].toArray(),depth+1);const int end=jump(bytes);for(int at:branches.second)link(bytes,at,bytes.size());
                body(n["else"].toArray(),depth+1);link(bytes,end,bytes.size());
            }else if(type=="flag") {load(bytes,0,number(n,"flag",0,767));load(bytes,1,number(n,"value",0,1,1));word(bytes,0x71,0x60);}
            else if(type=="scene") {load(bytes,0,number(n,"scene",0,16383));word(bytes,0x10);}
            else if(type=="recruit") {load(bytes,0,number(n,"template",0,555));word(bytes,0x16,number(n,"mode",0,1,1));word(bytes,0x13,22);}
            else if(type=="goth"||type=="days") {int amount=number(n,"amount",-32767,32767);if(type=="goth")amount=-amount;load(bytes,0,amount&255);word(bytes,type=="goth"?0x2e:0x17,(amount>>8)&255);}
            else if(type=="chaos") {load(bytes,0,number(n,"value",0,100,50));word(bytes,0x70,0xff);}
            else if(type=="program") {const int from=number(n,"clearFrom",0,255,200),to=number(n,"clearTo",0,255,255);if(from>to)throw Error("Flag range must be ascending");
                load(bytes,0,from|(to<<8));word(bytes,0x12,number(n,"program",1,114,4));}
            else if(type=="stop") {load(bytes,0,1);word(bytes,0x11);}
            else if(type=="return") {load(bytes,0,0);word(bytes,0x11);}
            else if(type=="continue")word(bytes,0x2f);
            else throw Error("Unknown authored action: "+type);
        }
    }
};
QVector<QVector<int>> tables(const QByteArray &source){
    if(source.size()<12||source.size()%2)throw Error("Invalid event program");QVector<QVector<int>> result(6);
    for(int group=0;group<6;++group){int at=u16(source,group*2);QSet<int> seen;
        for(;;at+=2){if(at<12||at+2>source.size()||at%2)throw Error("Unsupported event handler table");const int entry=u16(source,at);if(entry==65535)break;
            if(entry<12||entry+2>source.size()||entry%2||seen.contains(entry))throw Error("Invalid event handler entry");seen.insert(entry);result[group]<<entry;}}
    return result;
}
QByteArray compileAll(const QByteArray &source,int row,const QJsonArray &trees,const QJsonObject &changes,QMap<QString,int> *entries){
    if(trees.isEmpty()&&changes.isEmpty())return source;
    if(!EventAuthoring::supported(row))throw Error("This shared program does not use editable event triggers");
    auto groups=tables(source);Compiler c;c.bytes=source;QSet<int> changed;
    for(auto it=changes.begin();it!=changes.end();++it){bool ok=false;const int entry=it.key().toInt(&ok);const auto change=it.value().toObject();
        if(!ok||QString::number(entry)!=it.key()||!it.value().isObject())throw Error("Invalid trigger edit");
        const int group=number(change,"group",0,5),guard=number(change,"guard",-1,65534,-1),target=number(change,"target",-1,255,-1);
        int old=-1;for(int g=0;g<6;++g)if(groups[g].contains(entry)){if(old>=0)throw Error("Shared handlers cannot change triggers independently");old=g;}
        if(old<0)throw Error("Trigger handler is absent");
        const bool contact=group>=1&&group<=4;
        if(guard>=0){if(guard+8>source.size()||u8(source,guard)!=0x61||(u8(source,guard+1)!=0xf8&&u8(source,guard+1)!=0xfd)||
            u8(source,guard+2)!=0x6a||u16(source,guard+4)!=0xa601||u16(source,guard+6)!=0x2f00)throw Error("The event's original target guard has changed");
            if(contact&&target>=0){c.bytes[guard+1]=char(group<=2?0xf8:0xfd);c.bytes[guard+3]=char(target);}
            else { // Always take the existing true edge, without moving its body.
                c.bytes.replace(guard,6,QByteArray::fromHex("69006a00a601"));}
        }else if(contact&&target>=0)throw Error("This handler needs an explicit target condition; choose Any or create a new event");
        if(old!=group){groups[old].removeAll(entry);groups[group]<<entry;}changed.insert(old);changed.insert(group);
    }
    if(trees.size()>128)throw Error("A program supports up to 128 added events");QSet<QString> ids;QVector<QVector<int>> before(6),after(6);
    for(const auto &v:trees){if(!v.isObject())throw Error("Invalid event tree");const auto tree=v.toObject();const auto id=tree["id"].toString(),name=tree["name"].toString().trimmed();
        if(id.isEmpty()||ids.contains(id)||name.isEmpty()||name.size()>120||!tree["nodes"].isArray())throw Error("Each event needs a unique ID, name and steps");ids.insert(id);
        const int group=number(tree,"group",0,5),target=number(tree,"target",-1,255,-1);if(group<=2&&group>=1&&target==0)throw Error("Town IDs start at 1");
        const int entry=c.bytes.size();if(entries)(*entries)[id]=entry;c.count=0;c.group=group;
        if(group>=1&&group<=4&&target>=0){word(c.bytes,0x61,group<=2?0xf8:0xfd);load(c.bytes,2,target);word(c.bytes,0xa6,1);word(c.bytes,0x2f);}
        c.body(tree["nodes"].toArray());
        word(c.bytes,0x2f);(tree["after"].toBool()?after:before)[group]<<entry;changed.insert(group);
    }
    for(int group:changed){const int at=c.bytes.size();const auto list=before[group]+groups[group]+after[group];for(int entry:list)word(c.bytes,entry>>8,entry);word(c.bytes,255,255);if(at>65534)throw Error("Event table exceeds its native pointer range");put16(c.bytes,group*2,at);}
    if(c.bytes.size()>65534)throw Error("Event program exceeds its native pointer range");return c.bytes;
}
}
bool EventAuthoring::supported(int row){return row>=4&&row<=109&&row!=62&&row!=66&&row!=67&&row!=68&&row!=70;}
QMap<int,QString> EventAuthoring::triggers(){return {{0,"Mission entry"},{1,"Town visit · special event"},{2,"Town visit"},{3,"Squad encounter"},{4,"Character defeated"},{5,"Mission completion"}};}
QMap<QString,QString> EventAuthoring::conditions(){return {{"flag","Story flag"},{"chaos","Chaos Frame"},{"answer","Last dialogue answer"},{"squad","Encountered / defeated squad"},{"town","Visited town"},{"squadState","Contacting squad state"},{"all","All conditions (AND)"},{"any","Any condition (OR)"},{"not","Not (invert condition)"}};}
QMap<QString,QString> EventAuthoring::actions(){return {{"scene","Play cutscene"},{"flag","Set / clear story flag"},{"recruit","Recruit from template"},{"goth","Give / take Goth"},{"days","Advance / rewind calendar"},{"chaos","Set Chaos Frame"},{"program","Load event program"},{"stop","Stop processing this event trigger"},{"return","Return to gameplay"},{"continue","Continue with other events"}};}
QByteArray EventAuthoring::compile(const QByteArray &s,int row,const QJsonArray &t,const QJsonObject &r){return compileAll(s,row,t,r,nullptr);}
QMap<QString,int> EventAuthoring::entries(const QByteArray &s,int row,const QJsonArray &t,const QJsonObject &r){QMap<QString,int> out;compileAll(s,row,t,r,&out);return out;}
void EventAuthoring::validateReferences(const QJsonArray &trees,const QSet<int> &scenes,const QSet<int> &flags){
    std::function<void(QJsonValue,int)> walk=[&](QJsonValue value,int depth){if(depth>64)throw Error("Event nesting is too deep");
        if(value.isArray()){for(const auto &v:value.toArray())walk(v,depth+1);}
        else if(value.isObject()){const auto n=value.toObject();if(n["type"]=="scene"&&!scenes.contains(n["scene"].toInt()))throw Error("Event refers to a missing cutscene");
            if(n["type"]=="flag"&&!flags.contains(n["flag"].toInt(-1)))throw Error("Event refers to a missing or deleted flag");for(auto i=n.begin();i!=n.end();++i)if(i.value().isArray()||i.value().isObject())walk(i.value(),depth+1);}};
    walk(trees,0);
}
}
