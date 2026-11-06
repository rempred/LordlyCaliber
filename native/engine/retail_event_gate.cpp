#include "engine/retail_event_gate.h"
#include "engine/event_authoring.h"
#include "engine/event_script.h"
#include <QJsonArray>
#include <QSet>
namespace lordly {
namespace {
bool comparison(int opcode) {
    return opcode>=0xa0 && QSet<int>{1,2,3,6,7,11,12,13,14,15}.contains(opcode&15);
}
int groupAt(const QByteArray &retail,int row,int offset) {
    static thread_local QMap<QByteArray,QMap<int,QSet<int>>> owners;
    const auto key=sha256(retail)+QByteArray::number(row);
    if(!owners.contains(key)) {
        QMap<int,QSet<int>> groups;
        for(const auto &handler:EventScript::decode(retail,row))
            for(const auto &step:handler.steps)
                for(int at=step.offset;at<step.offset+step.length;at+=2)groups[at].insert(handler.group);
        owners.insert(key,groups);
    }
    const auto groups=owners.value(key).value(offset);
    return groups.size()==1?*groups.begin():-1;
}
int outcome(const QString &name,int yes,int no,int original) {
    if(name=="yes")return yes;
    if(name=="no")return no;
    if(name=="original")return original;
    throw Error("Choose Yes, No or the original condition as the branch destination");
}
struct Jump {int destination=0,mode=0;};
void word(QByteArray &bytes,int opcode,int operand=0) {
    bytes.append(char(opcode));bytes.append(char(operand));
}
}
bool RetailEventGate::supports(const QByteArray &retail,int row,int at) {
    if(!EventAuthoring::supported(row)||row<4||at<12||at%2||at+1>=retail.size()||!comparison(u8(retail,at)))return false;
    const auto offsets=EventScript::instructionOffsets(row);
    return offsets.contains(at)&&offsets.contains(at+2)&&
        offsets.contains(at+2+qint8(u8(retail,at+1))*2)&&groupAt(retail,row,at)>=0;
}
bool RetailEventGate::needsRuntime(const QJsonObject &rows) {
    for(auto row:rows)for(auto gate:row.toObject()["gates"].toObject())
        if(gate.toObject()["mode"]=="if")return true;
    return false;
}
int RetailEventGate::directory(const QByteArray &bytes) {
    if(bytes.size()<12)return -1;
    const int table=u16(bytes,0),directory=table-1028;
    return directory>=12&&table+2<=bytes.size()&&bytes.mid(directory,4)==QByteArray("LCJG")?directory:-1;
}
std::optional<int> RetailEventGate::destination(const QByteArray &bytes,int at) {
    if(at<12||at%2||at+2>bytes.size()||u8(bytes,at)!=8)return {};
    const int start=directory(bytes);if(start<0)return {};
    const int slot=start+4+u8(bytes,at+1)*4;
    const int target=u16(bytes,slot)*2;
    if(u8(bytes,slot+2)>2||u8(bytes,slot+3)||target<12||target>=start)return {};
    return target;
}
int RetailEventGate::transferMode(const QByteArray &bytes,int at) {
    if(!destination(bytes,at))return -1;
    return u8(bytes,directory(bytes)+4+u8(bytes,at+1)*4+2);
}
int RetailEventGate::originalComparison(const QByteArray &bytes,int at) {
    const auto target=destination(bytes,at);
    if(!target||transferMode(bytes,at)!=1||*target<18)return -1;
    const int original=*target-6;
    return comparison(u8(bytes,original))&&u8(bytes,original+1)==1&&
        transferMode(bytes,original+2)==0&&transferMode(bytes,original+4)==0?original:-1;
}
QByteArray RetailEventGate::compile(const QByteArray &compiled,const QByteArray &retail,
                                  int row,const QJsonObject &gates) {
    if(gates.isEmpty())return compiled;
    auto bytes=compiled;
    QVector<Jump> jumps;
    auto addJump=[&](int destination,int mode){
        if(jumps.size()>=256)throw Error("This event program supports 256 structural branch destinations");
        jumps.append({destination,mode});return jumps.size()-1;
    };
    for(auto it=gates.begin();it!=gates.end();++it) {
        bool valid=false;const int at=it.key().toInt(&valid);
        const auto gate=it.value().toObject();
        if(!valid||QString::number(at)!=it.key()||!it.value().isObject()||!supports(retail,row,at))
            throw Error("The selected instruction is not an editable original condition");
        if(at+2>bytes.size()||!comparison(u8(bytes,at)))throw Error("The original condition changed before structural editing");
        const int opcode=u8(bytes,at),operand=u8(bytes,at+1);
        const int yes=at+2+qint8(operand)*2,no=at+2;
        if(!EventScript::instructionOffsets(row).contains(yes))throw Error("The original condition has an unsupported edited destination");
        const auto mode=gate["mode"].toString();
        if(mode=="keep") {
            if(gate.size()!=2)throw Error("Invalid removed-condition settings");
            const auto selected=gate["outcome"].toString();
            if(selected=="yes"){put8(bytes,at,1);put8(bytes,at+1,quint8(operand));}
            else if(selected=="no"){put8(bytes,at,0);put8(bytes,at+1,0);}
            else throw Error("Choose the Yes or No branch to retain");
            continue;
        }
        if(mode!="if"||gate.size()!=4||!gate["condition"].isObject())throw Error("Invalid inserted-condition settings");
        const int group=groupAt(compiled,row,at);
        if(group<0||group>5)throw Error("This shared condition has no unambiguous event context");
        const int original=bytes.size();
        word(bytes,opcode,1);
        word(bytes,8,addJump(no,0));word(bytes,8,addJump(yes,0));
        const int trueDestination=outcome(gate["yes"].toString(),yes,no,original);
        const int falseDestination=outcome(gate["no"].toString(),yes,no,original);
        const int trueJump=addJump(trueDestination,2),falseJump=addJump(falseDestination,2);
        const int entry=bytes.size();
        bytes+=EventAuthoring::compileCondition(gate["condition"].toObject(),group,entry,trueJump,falseJump);
        put8(bytes,at,8);put8(bytes,at+1,quint8(addJump(entry,1)));
    }
    if(!jumps.isEmpty()) {
        // Header0 still addresses an ordinary native handler list. The custom
        // jump directory is immediately before its relocated copy.
        QVector<int> handlers;int position=u16(bytes,0);
        QSet<int> seen;
        while(position>=12&&position+2<=bytes.size()&&position%2==0) {
            if(seen.contains(position))throw Error("The mission-entry handler list loops");
            seen.insert(position);const int entry=u16(bytes,position);position+=2;
            if(entry==65535)break;
            if(entry<12||entry%2||entry+2>bytes.size())throw Error("The mission-entry handler list contains an invalid entry");
            handlers.append(entry);
        }
        if(position<14||position>bytes.size()||u16(bytes,position-2)!=65535)throw Error("The mission-entry handler list has no terminator");
        bytes.append("LCJG",4);const int directory=bytes.size();bytes.append(QByteArray(1024,0));
        for(int index=0;index<jumps.size();++index) {
            const auto &jump=jumps[index];
            if(jump.destination<12||jump.destination%2||jump.destination>=65534)throw Error("A condition destination exceeds its native cursor range");
            put16(bytes,directory+index*4,quint16(jump.destination/2));
            put8(bytes,directory+index*4+2,quint8(jump.mode));
        }
        const int table=bytes.size();
        for(int entry:handlers)word(bytes,entry>>8,entry&255);
        word(bytes,255,255);
        if(bytes.size()>65534)throw Error("The event program exceeds its native cursor range");
        put16(bytes,0,quint16(table));
    }
    return bytes;
}
}
