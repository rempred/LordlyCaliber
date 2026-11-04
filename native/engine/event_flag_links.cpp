#include "engine/event_flag_links.h"
#include "engine/event_authoring.h"
#include "engine/event_script.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>
#include <array>
#include <optional>
namespace lordly {
namespace {
const QJsonObject &catalog() {
    static const auto value=[] { QFile file(":/game/director/event-semantics.json");
        if(!file.open(QIODevice::ReadOnly))throw Error("Missing event descriptions");
        return QJsonDocument::fromJson(file.readAll()).object(); }();
    return value;
}
using Constant = std::optional<int>;
using Registers = std::array<Constant,8>;
struct Frame { int pc=0; Registers registers{}; QVector<Constant> stack; QVector<bool> returns; };
struct Root { int entry=-1,group=-1; QString title,id; };
bool comparison(int op) {
    return op>=0xa0&&QSet<int>{1,2,3,6,7,11,12,13,14,15}.contains(op&15);
}
QString address(int at) { return QString("0x%1").arg(at,4,16,QChar('0')).toUpper(); }
QString key(const Frame &frame) {
    QString result=QString::number(frame.pc);
    // Saved data values merge at a join, like registers. Only return cursors
    // distinguish call contexts; keying arbitrary saved values can explode on
    // loops that update a counter before saving it.
    for(int n=0;n<frame.stack.size();++n)result+='/'+(frame.returns[n]&&frame.stack[n]?QString::number(*frame.stack[n]):QString("v"));
    return result;
}
Constant arithmetic(int operation,Constant left,Constant right) {
    if(!left||!right)return {};
    const quint32 a=*left,b=*right;quint32 value=0;
    switch(operation) {case 0:value=a&b;break;case 1:value=a|b;break;
    case 2:value=a+b;break;case 3:value=a-b;break;case 4:value=a*b;break;
    case 5:if(!b)return {};value=a/b;break;case 6:if(!b)return {};value=a%b;break;
    default:return {};}
    return int(value&65535);
}
Constant branch(const Registers &r,int op) {
    const bool single=(op&15)>=12;
    const auto a=r[single?(op&3):((op>>2)&3)];
    const auto b=single?Constant{0}:r[op&3];
    if(!a||!b)return {};
    const int family=op>>4,x=family<12?*a:qint16(*a),y=family<12?*b:qint16(*b);
    switch(family){case 10:return x==y;case 11:return x!=y;case 12:return x>y;
    case 13:return x<y;case 14:return x>=y;case 15:return x<=y;default:return {};}
}
void transfer(Frame &frame,const QByteArray &bytes) {
    const int op=u8(bytes,frame.pc),arg=u8(bytes,frame.pc+1),d=op&7;
    auto &r=frame.registers;
    if(op>=0x68&&op<=0x6f)r[d]=arg;
    else if(op>=0x88&&op<=0x8f)r[d]=r[d]?Constant{(*r[d]&255)|(arg<<8)}:Constant{};
    else if(op>=0x60&&op<=0x67) {
        if(arg<8)r[d]=r[arg&7]?Constant{*r[arg&7]&255}:Constant{};
        else if(arg==0xf7){} // Native unhandled property preserves destination.
        else if((arg>>3)==26)r[d]=0;
        else r[d]={};
    }else if(op>=0x78&&op<=0x7f) {
        if(arg<0xe5||arg>=0xfd)r[d]={};
    }else if(op>=0x90&&op<=0x9f) {
        const bool immediate=op>=0x98;
        r[d]=arithmetic(arg>>(immediate?5:3),r[d],immediate?Constant{arg&31}:r[arg&7]);
    }else if(op==6) {
        r[arg&7]=frame.stack.isEmpty()?Constant{}:frame.stack.takeLast();
        if(!frame.returns.isEmpty())frame.returns.removeLast();
    }else if(op==0x28){r[6]={};r[7]={};}
    else if(op==0x10||op==0x11||op==0x13)r={};
}
QVector<Root> roots(const QByteArray &bytes,const QByteArray &retail,int row,
                    const QJsonObject &info,const SceneEvents &events,bool sourceQualified,
                    QSet<int> &tables) {
    QMap<int,Root> found;QMap<int,QString> titles;
    for(const auto &value:info["handlers"].toArray()) {
        const auto h=value.toObject();titles[h["entry"].toInt()]=h["title"].toString();
    }
    const auto offsets=EventScript::instructionOffsets(row);
    // Arrival/investigation and shared People programs have direct pointer tables.
    if(row==0||row==70||row==67) {
        const int header=offsets.isEmpty()?0:*std::min_element(offsets.begin(),offsets.end());
        for(int at=0;at+1<header;at+=2) {
            tables.insert(at);const int entry=u16(bytes,at);
            if(entry>0&&entry+1<bytes.size()&&entry%2==0&&
               (offsets.contains(entry)||entry>=retail.size()))found[entry]={entry,-1,{}, {}};
        }
    }else {
        const int groups=u16(bytes,0)==2?1:6;
        for(int group=0;group<groups;++group) {
            tables.insert(group*2);int at=u16(bytes,group*2);bool ended=false;
            for(;at>=groups*2&&at+1<bytes.size()&&at%2==0;at+=2) {
                // Retail training group3 deliberately has no sentinel. Its
                // first handler always yields before the native scheduler can
                // advance into code at0x12 (167 original scheduler/VM cases).
                if(row==68&&group==3&&at==0x12){ended=true;break;}
                tables.insert(at);const int entry=u16(bytes,at);
                if(entry==65535){ended=true;break;}
                if(entry<groups*2||entry+1>=bytes.size()||entry%2)
                    throw Error(QString("Event program %1 group %2 table %3 has invalid handler entry %4")
                        .arg(row).arg(group).arg(address(at),address(entry)));
                found[entry]={entry,group,{}, {}};
            }
            if(!ended)throw Error(QString("Event program %1 group %2 has invalid handler table at %3")
                .arg(row).arg(group).arg(address(at)));
        }
    }
    const auto rowEdits=events.flagEdits()[QString::number(row)].toObject();
    const auto trees=rowEdits["trees"].toArray();
    if(!trees.isEmpty()) {
        auto base=rowEdits;base.remove("trees");base.remove("triggers");auto edits=events.flagEdits();edits[QString::number(row)]=base;
        const auto positions=EventAuthoring::entries(SceneEvents::flagRow(retail,row,edits),row,trees,rowEdits["triggers"].toObject());
        for(const auto &value:trees) {
            const auto tree=value.toObject();const auto id=tree["id"].toString();const int entry=positions.value(id,-1);
            if(found.contains(entry)){found[entry].id=id;found[entry].title=tree["name"].toString();}
        }
    }
    QVector<Root> result;
    for(auto root:found) {
        if(root.title.isEmpty())root.title=sourceQualified?titles.value(root.entry):QString{};
        if(root.title.isEmpty())root.title=QString("%1 · event %2").arg(root.group<0?QString("Shared story logic"):EventScript::groupName(root.group),address(root.entry));
        result<<root;
    }
    return result;
}
}
static QVector<EventFlagWriter> scan(const Rom &rom,const SceneEvents &events,
                                    const ScenePrograms &scenes,int contextRow,int flag,
                                    bool includeProgramResets,int onlyRow=-1) {
    QVector<EventFlagWriter> result;
    const bool all=onlyRow>=0;
    if((flag<0&&!all)||flag>=SceneEvents::firstCustomFlag+SceneEvents::customFlagCapacity)return result;
    const bool local=(flag>=170&&flag<=173)||(flag>=200&&flag<=254);
    const auto rows=catalog()["rows"].toObject();
    for(auto rowIt=rows.begin();rowIt!=rows.end();++rowIt) {
        const int row=rowIt.key().toInt();
        if(all&&row!=onlyRow)continue;
        if(local&&row!=contextRow) {
            if(!includeProgramResets)continue;
            bool switches=false;
            for(const auto &value:rowIt.value().toObject()["steps"].toArray())
                if((value.toArray()[3].toInt()>>8)==0x12){switches=true;break;}
            if(!switches&&events.flagEdits()[rowIt.key()].toObject()["trees"].toArray().isEmpty())continue;
        }
        // Custom flags are written only by authored event programs. Avoid
        // decoding the whole retail corpus for an unused custom flag.
        if(flag>=256&&events.flagEdits()[rowIt.key()].toObject()["trees"].toArray().isEmpty())continue;
        const auto retail=SceneEvents::retailRow(rom,row),bytes=events.program(row);
        const auto info=rowIt.value().toObject();QSet<int> tables;
        const bool qualified=bytes==retail&&!scenes.modified();
        const auto handlers=roots(bytes,retail,row,info,events,qualified,tables);
        auto allowed=EventScript::instructionOffsets(row);
        for(int at=retail.size();at+1<bytes.size();at+=2)if(!tables.contains(at))allowed.insert(at);
        for(int at:tables)allowed.remove(at);
        for(const auto &root:handlers) {
            QMap<QString,Frame> inputs;QVector<QString> pending;QSet<QString> queued;
            Frame initial;initial.pc=root.entry;const auto initialKey=key(initial);
            inputs[initialKey]=initial;pending<<initialKey;queued.insert(initialKey);
            int iterations=0;const int budget=qMax(4096,allowed.size()*128);
            while(!pending.isEmpty()) {
                if(++iterations>budget)throw Error(QString("Flag writer analysis exceeded its control-flow limit in program %1 event %2")
                    .arg(row).arg(address(root.entry)));
                const auto id=pending.takeLast();queued.remove(id);auto frame=inputs[id];const int at=frame.pc;
                if(!allowed.contains(at)||at<0||at+1>=bytes.size())continue;
                const int op=u8(bytes,at),arg=u8(bytes,at+1);const auto before=frame.registers;
                transfer(frame,bytes);
                QVector<int> next;
                if(op==1)next<<at+2+qint8(arg)*2;
                else if(comparison(op)) {
                    const auto taken=branch(before,op);
                    if(!taken||*taken)next<<at+2+qint8(arg)*2;
                    if(!taken||!*taken)next<<at+2;
                }else if(op==0x2d)next<<at+2<<at+2+qint8(arg)*2;
                else if(op==2||op==3) {
                    if(before[0]) {
                        if(op==3){if(frame.stack.size()>=32)continue;frame.stack<<Constant{at/2};frame.returns<<true;}
                        next<<int(quint16((at+2)/2+(arg<<8)+(*before[0]&255)))*2;
                    }
                }else if(op==4) {
                    if(!frame.stack.isEmpty()){const auto cursor=frame.stack.takeLast();frame.returns.removeLast();if(cursor)next<<(*cursor+1)*2;}
                }else if(op==5) {
                    if(frame.stack.size()>=32)continue;frame.stack<<before[arg&7];frame.returns<<false;next<<at+2;
                }else if(op!=0x11&&op!=0x12&&op!=0x2f)next<<at+2;
                for(int destination:next) {
                    if(!allowed.contains(destination))continue;
                    auto after=frame;after.pc=destination;const auto target=key(after);bool changed=false;
                    if(!inputs.contains(target)){
                        if(inputs.size()>=32768)throw Error(QString("Flag writer analysis exceeded its context limit in program %1 event %2 at %3")
                            .arg(row).arg(address(root.entry),address(destination)));
                        inputs[target]=after;changed=true;
                    }else {
                        for(int r=0;r<8;++r)if(inputs[target].registers[r]&&inputs[target].registers[r]!=after.registers[r]){inputs[target].registers[r]={};changed=true;}
                        for(int n=0;n<after.stack.size();++n)if(inputs[target].stack[n]&&inputs[target].stack[n]!=after.stack[n]){inputs[target].stack[n]={};changed=true;}
                    }
                    if(changed&&!queued.contains(target)){pending<<target;queued.insert(target);}
                }
            }
            // Read only converged register states. An early predecessor may
            // temporarily suggest an exact target that a later merge invalidates.
            for(const auto &frame:inputs) {
                const int at=frame.pc;if(!allowed.contains(at))continue;
                const int op=u8(bytes,at),arg=u8(bytes,at+1);const auto &before=frame.registers;
                auto record=[&](int writtenFlag,int state,bool reset) {
                    for(auto &existing:result)if(existing.row==row&&existing.entry==root.entry&&existing.offset==at&&existing.flag==writtenFlag&&existing.programReset==reset) {
                        if(existing.state!=state)existing.state=-1;return;
                    }
                    EventFlagWriter writer;writer.row=row;writer.offset=at;writer.entry=root.entry;writer.flag=writtenFlag;writer.state=state;
                    writer.title=root.title;writer.authoredId=root.id;writer.programReset=reset;
                    writer.detail=reset?QString("Loading event program %1 clears flags %2 through %3 (upper bound excluded). This is program-reset behavior.").arg(arg).arg(*before[0]&255).arg(*before[0]>>8)
                        :QString("%1 %2 at %3").arg(state==1?"Set":state==0?"Clear":"Conditionally set or clear",events.flagName(writtenFlag,row,qualified?at:-1),address(at));
                    result<<writer;
                };
                if((!local||row==contextRow)&&op>=0x70&&op<=0x77&&(arg>>3)==12&&before[arg&7]&&
                   (all||*before[arg&7]==flag)&&*before[arg&7]>=0&&*before[arg&7]<768)
                    record(*before[arg&7],before[op&7]?(*before[op&7]==1?1:0):-1,false);
                if(includeProgramResets&&op==0x12&&(!local||row==contextRow||arg==contextRow)&&before[0]&&flag>=(*before[0]&255)&&flag<(*before[0]>>8))record(flag,0,true);
            }
        }
    }
    std::sort(result.begin(),result.end(),[](const auto &a,const auto &b){if(a.programReset!=b.programReset)return !a.programReset;
        if(a.row!=b.row)return a.row<b.row;if(a.entry!=b.entry)return a.entry<b.entry;return a.offset<b.offset;});
    return result;
}
QVector<EventFlagWriter> EventFlagLinks::writers(const Rom &rom,const SceneEvents &events,
                                               const ScenePrograms &scenes,int contextRow,int flag,
                                               bool includeProgramResets) {
    return scan(rom,events,scenes,contextRow,flag,includeProgramResets);
}
QVector<EventFlagWriter> EventFlagLinks::writersInProgram(const Rom &rom,const SceneEvents &events,
                                                       const ScenePrograms &scenes,int row) {
    return scan(rom,events,scenes,row,-1,false,row);
}
}
