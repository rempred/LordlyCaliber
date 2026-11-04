#include "engine/event_flow.h"
#include "engine/event_authoring.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>
#include <array>
namespace lordly {
namespace {
const QJsonObject &catalog() {
    static const auto result=[] {QFile f(":/game/director/event-semantics.json");
        if(!f.open(QIODevice::ReadOnly))throw Error("Missing event descriptions");
        return QJsonDocument::fromJson(f.readAll()).object();}();return result;
}
QMap<int,QString> names(const QString &kind,const QString &scope="global") {
    static const auto entries=[] {QMap<QString,QMap<int,QString>> out;
        for(const auto &v:catalog()["names"].toArray()){const auto a=v.toArray();out[a[0].toString()+"/"+a[1].toString()][a[2].toInt()]=a[3].toString();}return out;}();
    return entries.value(kind+"/"+scope);
}
QString reference(const QString &kind,int id,const QString &scope="global") {
    const auto name=names(kind,scope).value(id);return name.isEmpty()?QString("%1 ID %2").arg(kind).arg(id):name;
}
QString trigger(int group) {
    switch(group){case 0:return "Mission entry";case 1:return "Town visit · special event";
    case 2:return "Town visit";case 3:return "Squad encounter";case 4:return "Character defeated";
    case 5:return "Mission completion";default:return "Shared story logic";}
}
bool conditional(int op){return op>=0xa0&&QSet<int>{1,2,3,6,7,11,12,13,14,15}.contains(op&15);}
bool pureProducer(int op){return (op>=0x60&&op<=0x6f)||(op>=0x78&&op<=0x7f)||(op>=0x88&&op<=0x9f);}
QString relation(int op){switch(op>>4){case 0xa:return "=";case 0xb:return "≠";case 0xc:return ">";case 0xd:return "<";case 0xe:return "≥";default:return "≤";}}
QString requestName(int value){return catalog()["requests"].toObject()[QString::number(value)].toObject()["name"].toString(QString("Game operation ID %1").arg(value));}
QMap<int,QString> requestChoices(){QMap<int,QString> result;for(int n=0;n<256;++n)result[n]=requestName(n);return result;}
struct Expression {
    QString text, kind;
    int value=-1, property=-1, bank=-1;
    bool byte=false,direct=false;
    QVector<Expression> children;
    QVector<StoryField> fields;
};
using Registers=std::array<Expression,8>;
QString expressionIdentity(const Expression &e){
    QString result=e.kind+":"+e.text+":"+QString::number(e.value)+":"+QString::number(e.property)+":"+QString::number(e.bank)+":"+QString::number(e.direct);
    for(const auto &f:e.fields)result+=QString("/%1:%2:%3").arg(f.offset).arg(f.key).arg(f.value);
    return result;
}
void appendFields(QVector<StoryField> &to,const QVector<StoryField> &from){
    for(const auto &f:from){bool present=false;for(const auto &existing:to)present|=existing.offset==f.offset&&existing.key==f.key&&existing.mode==f.mode;
        if(!present)to<<f;}
}
Expression literalExpression(const QByteArray &bytes,const QSet<int> &offsets,int at){
    const int op=u8(bytes,at);const bool wide=offsets.contains(at+2)&&u8(bytes,at+2)==op+0x20;
    const int value=u8(bytes,at+1)|(wide?u8(bytes,at+3)<<8:0);
    // A wide editor field spans both instructions, but the VM first executes
    // only the low-byte assignment. The following upper-byte transfer performs
    // the combination on its actual incoming value (including edited jumps).
    Expression e{QString::number(u8(bytes,at+1)),"literal",u8(bytes,at+1)};e.byte=true;e.direct=true;
    e.fields<<StoryField{"Value","value",at,value,0,wide?65535:255};return e;
}
QString propertyLabel(int property,int group){
    if(property==0xfd){if(group==4)return "Defeated squad";if(group==3)return "Encountered squad";if(group==1||group==2)return "Contacted town (zero-based)";}
    if(property==0xfe){if(group==1||group==2)return "Visiting squad";if(group==3)return "Contacting squad";}
    const auto original=catalog()["properties"].toObject()[QString::number(property)].toObject()["name"].toString();
    return original.isEmpty()?QString("Property ID %1").arg(property):original;
}
Expression readExpression(const QByteArray &bytes,int at,const Registers &r,int row,int group,bool scopedNames){
    const int op=u8(bytes,at),arg=u8(bytes,at+1);const bool alternate=op>=0x78;
    if(!alternate&&arg>=0xe4){
        if(arg==0xf7)return r[op&7];
        Expression e{propertyLabel(arg,group),"property"};e.property=arg;
        e.byte=arg!=0xf8&&catalog()["properties"].toObject()[QString::number(arg)].toObject()["storage"].toObject()["bytes"].toInt()==1;return e;
    }
    if(!alternate&&arg<8){auto e=r[arg&7];if(e.text.isEmpty()||e.byte)return e;
        if(e.kind=="literal"){e.value&=255;e.text=QString::number(e.value);e.byte=true;e.direct=false;return e;}
        Expression low{"Low byte of "+e.text,"low"};low.byte=true;low.children<<e;low.fields=e.fields;return low;}
    if(alternate&&arg>=0xe5){
        if(arg<0xfd)return r[op&7];
        Expression e{arg==0xfd?"Clock hour":arg==0xfe?"Clock minute":"Clock second","clock"};e.byte=true;return e;
    }
    const int bank=arg>>3;const auto entries=catalog()["banks"].toObject()[alternate?"altGetters":"getters"].toArray();
    if(bank>=entries.size())return {};
    const auto definition=entries[bank].toObject();
    if(definition["status"]!="verified")return {};
    if(!alternate&&bank==26){Expression e{"0","literal",0};e.byte=true;return e;}
    auto index=r[arg&7];if(index.text.isEmpty())return {};
    Expression e;e.kind=alternate?"alternate":"service";e.bank=bank;e.children<<index;e.fields=index.fields;
    if(!alternate&&bank==12&&index.kind=="literal"){
        const int token=qint16(index.value),id=qAbs(token);
        if(id>255)return {};
        e.kind="flag";e.value=token;e.text=EventScript::flagName(id,row,scopedNames?at:-1);e.byte=true;
        if(index.direct&&index.fields.size()==1&&index.fields.first().key=="value")
            for(auto &f:e.fields){f.label="Flag";f.kind="flag";f.choices.clear();
                if(token>=0)for(int n=0;n<256&&n<=f.maximum;++n)f.choices[n]=EventScript::flagName(n,row);}
        return e;
    }
    if(!alternate&&bank==1){
        const auto squad=r[7];if(squad.text.isEmpty())return {};
        e.kind="member";e.byte=true;e.children<<squad;appendFields(e.fields,squad.fields);
        e.text=index.kind=="literal"&&index.value==0?"First member of "+squad.text.toLower():QString("Member %1 of %2").arg(index.text,squad.text.toLower());
        if(index.direct&&index.fields.size()==1)for(auto &f:e.fields)if(f.offset==index.fields.first().offset)f.label="Member position (0 = first)";
        return e;
    }
    if(alternate&&(bank==1||bank==5)){
        e.kind=bank==1?"consumable-slot":"equipment-slot";
        const QString kind=bank==1?"consumable":"equipment";
        e.text=(bank==1?"Inventory slot for consumable · ":"Inventory slot for equipment · ")+(index.kind=="literal"?reference(kind,index.value):index.text);
        if(index.direct&&index.fields.size()==1&&index.fields.first().key=="value")
            for(auto &f:e.fields){f.label=bank==1?"Consumable":"Equipment";f.kind=kind;f.choices=names(kind);}
        return e;
    }
    e.byte=definition["storage"].toObject()["type"].toString()=="u8"||definition["storage"].toObject()["type"].toString()=="bit";
    e.text=definition["name"].toString()+" · "+index.text;
    if(!alternate&&bank==13){e.kind="squad-state";e.text=index.text;}
    if(!alternate&&((bank>=2&&bank<=6)||(bank>=7&&bank<=11))){
        const auto side=bank<=6?QString("army roster"):QString("opposing roster");
        const int role=bank<=6?bank-2:bank-7;
        e.kind=role==0?"hp":role==3?"status":"character";
        const QString name=role==0?"HP":role==1?"Identity class":role==2?"Current class":role==3?"Status":"Name category";
        e.text=QString("%1 of %2 (%3)").arg(name,index.text.toLower(),side);
        if(role==1||role==2)e.kind=role==1?"identity-class":"current-class";
        if(role==4)e.kind="name-category";
    }
    if(!alternate&&bank==18&&index.kind=="literal"&&index.value==3){e.kind="answer";e.text="Last dialogue answer";}
    if(!alternate&&bank==27){e.kind="temporary-flag";e.text="Temporary gameplay flag · "+index.text;for(auto &f:e.fields)if(f.label=="Value")f.label="Temporary flag ID";}
    return e;
}
Expression arithmeticExpression(int operation,const Expression &left,const Expression &right){
    if(left.text.isEmpty()||right.text.isEmpty())return {};
    const QStringList signs{"&","|","+","−","×","÷","remainder"};
    if(operation<0||operation>=signs.size())return {};
    Expression e{QString("(%1 %2 %3)").arg(left.text,signs[operation],right.text),"calculation"};
    e.value=operation;e.children={left,right};e.fields=left.fields;appendFields(e.fields,right.fields);
    if(e.text.size()>600)return {};
    if(left.kind=="literal"&&right.kind=="literal"){
        const int a=left.value,b=right.value;quint32 value=0;
        if((operation==5||operation==6)&&!b)return {};
        switch(operation){case 0:value=a&b;break;case 1:value=a|b;break;case 2:value=a+b;break;case 3:value=a-b;break;case 4:value=quint32(a)*quint32(b);break;case 5:value=a/b;break;default:value=a%b;break;}
        e.kind="literal";e.value=value&65535;e.text=QString::number(e.value);e.byte=e.value<256;
    }
    return e;
}
Registers transfer(const QByteArray &bytes,const QSet<int> &offsets,int at,Registers r,int row,int group,bool scopedNames){
    const int op=u8(bytes,at),arg=u8(bytes,at+1),d=op&7;
    if(op>=0x68&&op<=0x6f)r[d]=literalExpression(bytes,offsets,at);
    else if((op>=0x60&&op<=0x67)||(op>=0x78&&op<=0x7f))r[d]=readExpression(bytes,at,r,row,group,scopedNames);
    else if(op>=0x88&&op<=0x8f){
        if(r[d].kind=="literal"){
            const bool combined=r[d].direct&&r[d].fields.size()==1&&r[d].fields.first().offset==at-2&&
                r[d].fields.first().key=="value"&&offsets.contains(at-2)&&u8(bytes,at-2)==op-0x20;
            r[d].value=(r[d].value&255)|(arg<<8);r[d].text=QString::number(r[d].value);r[d].byte=false;
            if(!combined){r[d].fields<<StoryField{"Upper byte","value",at,arg,0,255};r[d].direct=false;}
        }else r[d]={};
    }else if(op>=0x90&&op<=0x9f){
        const bool immediate=op>=0x98;Expression rhs;
        if(immediate){rhs={QString::number(arg&31),"literal",arg&31};rhs.fields<<StoryField{"Amount","argument",at,arg&31,0,31};}
        else rhs=r[arg&7];
        r[d]=arithmeticExpression(arg>>(immediate?5:3),r[d],rhs);
    }else if(op==6)r[arg&7]={};
    else if(op==0x28){r[6]={};r[7]={};}
    else if(op==2||op==3||op==4||op==0x10||op==0x11||op==0x13)r={};
    return r;
}
QMap<int,Registers> registerInputs(const QByteArray &bytes,const QSet<int> &offsets,const QJsonArray &handlers,int row,bool scopedNames){
    QMap<int,Registers> inputs;QMap<int,int> groups;QVector<int> pending;QSet<int> queued;
    for(const auto &value:handlers){const auto h=value.toObject();const int at=h["entry"].toInt();inputs[at]={};groups[at]=h["group"].toInt(-1);pending<<at;queued.insert(at);}
    int iterations=0;const int maximum=qMax(1000,offsets.size()*24);
    while(!pending.isEmpty()&&iterations++<maximum){
        const int at=pending.takeLast();queued.remove(at);if(!offsets.contains(at))continue;
        const int op=u8(bytes,at),arg=u8(bytes,at+1);auto after=transfer(bytes,offsets,at,inputs[at],row,groups.value(at,-1),scopedNames);
        QVector<int> next;
        if(op==1||op==0x2d||conditional(op)){next<<at+2+qint8(arg)*2;if(op!=1)next<<at+2;}
        else if(op==2||op==3){
            const auto r0=inputs[at][0];if(r0.kind=="literal")next<<quint16((at+2)/2+(arg<<8)+(r0.value&255))*2;
            if(op==3)next<<at+2;
        }else if(op!=4&&op!=0x12&&op!=0x2f&&op!=0x11)next<<at+2;
        for(int to:next){if(!offsets.contains(to))continue;bool changed=false;
            if(!inputs.contains(to)){inputs[to]=after;groups[to]=groups.value(at,-1);changed=true;}
            else for(int r=0;r<8;++r)if(!inputs[to][r].text.isEmpty()&&expressionIdentity(inputs[to][r])!=expressionIdentity(after[r])){inputs[to][r]={};changed=true;}
            if(changed&&!queued.contains(to)){pending<<to;queued.insert(to);}
        }
    }
    if(!pending.isEmpty())for(auto &r:inputs)r={};
    return inputs;
}
StoryCondition comparisonCondition(const Expression &left,const Expression &right,int op){
    StoryCondition c;if(left.text.isEmpty()||right.text.isEmpty())return c;
    const auto label=[op](const Expression &e){
        if(e.kind=="literal"&&(op>>4)>=12)return QString::number(qint16(e.value));
        return e.kind=="flag"&&e.value<0?"Inverse of "+e.text:e.text;
    };
    c.operation="test";c.title=label(left)+" "+relation(op)+" "+label(right);c.fields=left.fields;appendFields(c.fields,right.fields);
    for(auto &f:c.fields)if(f.label=="Value")f.label="Compared value";
    if(right.kind=="literal"&&right.direct&&right.fields.size()==1&&!left.text.isEmpty())
        for(auto &f:c.fields)if(f.offset==right.fields.first().offset)f.label=left.kind=="hp"?"HP":left.kind=="answer"?"Answer ID":left.kind=="property"?left.text:"Compare against";
    if(right.kind=="literal"&&right.direct&&right.fields.size()==1){
        for(auto &f:c.fields)if(f.offset==right.fields.first().offset){
            if(left.kind=="identity-class"||left.kind=="current-class"){f.label=left.kind=="identity-class"?"Identity class":"Current class";f.kind="identity-class";f.choices=names("identity-class");}
            if(left.kind=="name-category"){f.label="Naming category";f.kind="named-state";f.choices={{0,"Male naming route"},{1,"Female naming route"},{2,"Third naming route"}};}
        }
    }
    if((op>>4==10||op>>4==11)&&left.kind=="calculation"&&left.value==0&&left.children.size()==2&&
       left.children[0].kind=="squad-state"&&left.children[1].kind=="literal"&&left.children[1].direct&&left.children[1].fields.size()==1&&
       right.kind=="literal"&&right.direct&&right.fields.size()==1&&!(left.children[1].value&~129)&&!(right.value&~left.children[1].value)){
        const auto mask=left.children[1];c.fields=left.children[0].fields;QStringList states;
        for(int bit:{1,128}){auto f=mask.fields.first();f.label=bit==1?"Squad formation":"Squad side";f.mode=StoryField::SquadState;f.kind="named-state";
            f.location=right.fields.first().offset;f.mask=bit;f.minimum=-1;f.maximum=1;f.value=mask.value&bit?(right.value&bit?1:0):-1;
            f.choices={{-1,"Either"},{0,bit==1?"Absent":"Friendly"},{1,bit==1?"Present":"Hostile"}};c.fields<<f;
            if(f.value>=0)states<<f.choices[f.value].toLower();}
        c.title=left.children[0].text+(op>>4==10?" matches: ":" does not match: ")+(states.isEmpty()?"any state":states.join(" and "));
    }
    if((op>>4==10||op>>4==11)&&right.kind=="literal"&&right.value==0){
        const bool nonzero=op>>4==11;
        if(left.kind=="flag")c.title=QString("%1 is %2").arg(left.text,(nonzero==(left.value>=0))?"ON":"OFF");
        else if(left.kind=="member"&&left.children.size()>1){
            if(left.children[0].kind=="literal"&&left.children[0].value==0)
                c.title=left.children[1].text+(nonzero?" has a first member":" has no first member");
            else c.title=left.text+(nonzero?" is present":" is absent");
        }
        else if(left.kind=="hp")c.title=left.text+(nonzero?" is above 0":" is 0");
        else if(left.kind=="calculation"&&left.value==0&&left.children.size()==2&&left.children[1].kind=="literal"&&left.children[1].value==4&&left.children[0].kind=="status"){
            const auto subject=left.children[0].children.value(0).text;
            c.title=subject+(nonzero?" is petrified":" is not petrified");
            // The4 is the game's Petrification mask, not a tunable amount.
            c.fields=left.children[0].fields;
        }
    }
    return c;
}
}
QVector<StoryEvent> EventFlow::read(const Rom &rom,const SceneEvents &events,const ScenePrograms &scenes,int row,bool originalSceneText) {
    const auto info=catalog()["rows"].toObject()[QString::number(row)].toObject();
    if(info.isEmpty())return {};
    const auto retail=SceneEvents::retailRow(rom,row);
    const auto bytes=SceneEvents::flagRow(retail,row,events.flagEdits());
    const auto offsets=EventScript::instructionOffsets(row);
    auto nativeHandlers=info["handlers"].toArray();
    const auto triggerChanges=events.flagEdits()[QString::number(row)].toObject()["triggers"].toObject();
    for(int n=0;n<nativeHandlers.size();++n){auto h=nativeHandlers[n].toObject();const auto change=triggerChanges[QString::number(h["entry"].toInt())].toObject();if(!change.isEmpty()){h["group"]=change["group"];nativeHandlers[n]=h;}}
    const bool scopedNames=originalSceneText&&!scenes.modified()&&bytes==retail&&events.json()["bindings"].toObject().isEmpty();
    const auto inputs=registerInputs(bytes,offsets,nativeHandlers,row,scopedNames);
    QSet<int> entrances;QMap<int,QSet<int>> incoming;
    for(const auto &h:nativeHandlers){const int entry=h.toObject()["entry"].toInt();entrances.insert(entry);incoming[entry].insert(-1);}
    // The audited VM also follows non-adjacent producers and local calls. Keep
    // those entry edges in addition to branches decoded from the current bytes.
    for(const auto &v:info["edges"].toArray()){const auto edge=v.toArray();entrances.insert(edge[1].toInt());incoming[edge[1].toInt()].insert(edge[0].toInt());}
    for(int at:offsets){const int op=u8(bytes,at),arg=u8(bytes,at+1);
        if(op==1||op==0x2d||conditional(op)){const int target=at+2+qint8(arg)*2;entrances.insert(target);incoming[target].insert(at);}
        if((op==2||op==3)&&offsets.contains(at-2)&&u8(bytes,at-2)==0x68){
            const int target=quint16((at+2)/2+(arg<<8)+u8(bytes,at-1))*2;entrances.insert(target);incoming[target].insert(at);}
    }
    // Unknown/dynamic call destinations cannot be erased: the projection never
    // moves instructions and raw navigation retains every owned word.
    QMap<int,SceneEventSite> launches;for(const auto &s:SceneEvents::sites())if(s.row==row)launches[s.offset]=s;
    QMap<int,QString> sceneNames;
    for(const auto &id:scenes.scenes())for(int selector:scenes.resource(id).selectors)sceneNames[selector]=scenes.displayName(id);
    if(originalSceneText&&!scenes.modified()){const auto known=names("scene");for(auto i=known.begin();i!=known.end();++i)if(sceneNames.contains(i.key()))sceneNames[i.key()]=i.value();}
    QMap<int,EventStep> flagControls;try{flagControls=EventScript::flagSteps(SceneEvents::retailRow(rom,row));}catch(const Error &){}
    QMap<int,QString> towns;
    const auto keys=info["scenarioKeys"].toArray();
    if(keys.size()==1)towns=names("scenario-site",QString("scenario:%1").arg(keys.first().toInt()));
    auto field=[&](int at,QString key,QString label,QString kind=QString{}) {
        for(const auto &p:EventScript::parameters(bytes,row,at))if(p.key==key){
            StoryField f{label,key,at,p.value,p.minimum,p.maximum,p.choices,kind.isEmpty()?p.kind:kind};
            if(!kind.isEmpty()){f.choices=names(kind);for(auto i=f.choices.begin();i!=f.choices.end();)if(i.key()<f.minimum||i.key()>f.maximum)i=f.choices.erase(i);else ++i;}
            return f;
        }
        throw Error("Grouped event value is not editable at its source instruction");
    };
    QVector<StoryEvent> result;
    for(int h=0;h<nativeHandlers.size();++h){const auto entry=nativeHandlers[h].toObject();
        StoryEvent event;event.entry=entry["entry"].toInt();event.group=entry["group"].toInt(-1);event.trigger=trigger(event.group);
        if(entry["group"]=="world-location")event.trigger=row==70?"Choose Area Investigation":"Arrive at map location";
        else if(row==1)event.trigger="New campaign";
        else if(row==62)event.trigger="Ending sequence";
        else if(row==66)event.trigger="History playback";
        else if(row==68)event.trigger="Training encounter";
        else if(row>=110)event.trigger="Chapter introduction";
        const int end=h+1<nativeHandlers.size()?nativeHandlers[h+1].toObject()["entry"].toInt():info["byteLength"].toInt();
        bool original=originalSceneText&&bytes.mid(event.entry,end-event.entry)==retail.mid(event.entry,end-event.entry);
        for(const auto &launch:launches)if(launch.offset>=event.entry&&launch.offset<end&&events.target(launch.id)!=launch.selector)original=false;
        event.title=original?entry["title"].toString():event.trigger+QString(" · event %1").arg(h+1);
        auto owns=[&](int at,int count){for(int n=0;n<count;++n)if(at+n*2>=end||!offsets.contains(at+n*2)||(n&&entrances.contains(at+n*2)))return false;return true;};
        auto closed=[&](int at,int count){for(int n=0;n<count;++n){const int pc=at+n*2;if(pc>=end||!offsets.contains(pc))return false;
            if(n)for(int from:incoming.value(pc))if(from<at||from>=at+count*2)return false;}return true;};
        auto word=[&](int at){return at>=0&&at+1<bytes.size()?int(u16(bytes,at)):-1;};
        int lastScene=-1;
        for(int at=event.entry;at<end;){if(!offsets.contains(at)){at+=2;continue;}
            if(entrances.contains(at))lastScene=-1;
            const int op=u8(bytes,at),arg=u8(bytes,at+1);
            StoryAction action;int count=1;
            auto consume=[&](int n){count=n;for(int k=0;k<n;++k)action.instructions<<at+k*2;};
            // A guarded town handler: failure returns immediately; success falls
            // through to the action. The interior true edge is owned explicitly.
            static const auto visitingUnit=QByteArray::fromHex("67fe606f6a8190026a01b2116a00660a6b1e6007e3066016ac0a602e9804bc070107603eac0460569804bc0101012f00");
            auto visitorShape=[&]{if((event.group!=1&&event.group!=2)||!closed(at,24))return false;
                const QSet<int> values{5,9,13,24,25,30,31,36,37,42,43};
                for(int i=0;i<48;++i)if(!values.contains(i)&&bytes[at+i]!=visitingUnit[i])return false;
                if((u8(bytes,at+5)&~129)||(u8(bytes,at+9)&~u8(bytes,at+5))||u8(bytes,at+13)>4)return false;
                for(const auto pair:{qMakePair(24,36),qMakePair(30,42)}){
                    const int op=u8(bytes,at+pair.first);if((op!=0xac&&op!=0xbc)||u8(bytes,at+pair.second)!=op)return false;
                    const bool first=u8(bytes,at+pair.first+1)==0,second=u8(bytes,at+pair.second+1)==0;if(first!=second)return false;
                    for(int p:{pair.first,pair.second})if(u8(bytes,at+p+1)!=0&&bytes[at+p+1]!=visitingUnit[p+1])return false;
                }return true;};
            if(visitorShape()){
                action.kind="requirement";action.title="Visiting squad and selected member";action.condition.operation="all";action.condition.title=action.title;
                StoryCondition squad{"test","Visiting squad"};
                for(int bit:{1,128}){auto f=field(at+4,"value",bit==1?"Formation":"Side");f.mode=StoryField::SquadState;f.kind="named-state";f.location=at+8;f.mask=bit;f.minimum=-1;f.maximum=1;
                    f.value=u8(bytes,at+5)&bit?(u8(bytes,at+9)&bit?1:0):-1;f.choices={{-1,"Either"},{0,bit==1?"Absent":"Friendly"},{1,bit==1?"Present":"Hostile"}};squad.fields<<f;}
                action.condition.children<<squad;
                StoryCondition member{"test","Check squad member"};auto who=field(at+12,"value","Member");who.choices={{0,"First member"},{1,"Second member"},{2,"Third member"},{3,"Fourth member"},{4,"Fifth member"}};who.kind="named-state";member.fields<<who;
                for(const auto pair:{qMakePair(24,36),qMakePair(30,42)}){StoryField f;f.label=pair.first==24?"HP":"Petrification";f.key="requirement";f.offset=at+pair.first;f.location=at+pair.second;f.rule=at+46;f.mode=StoryField::VisitorCheck;f.kind="named-state";f.minimum=-1;f.maximum=1;
                    f.value=u8(bytes,f.offset+1)==0?-1:u8(bytes,f.offset)==0xac?1:0;
                    f.choices={{-1,"Either"},{0,pair.first==24?"Zero HP":"Not petrified"},{1,pair.first==24?"Above zero":"Petrified"}};member.fields<<f;}
                action.condition.children<<member;consume(24);
            }else if(closed(at,4)&&op==0x69&&word(at+2)==0x6061&&(word(at+4)==0xac01||word(at+4)==0xbc01)&&
                (word(at+6)==0x2f00||(closed(at,6)&&word(at+6)==0x0101&&word(at+8)==0x0101&&word(at+10)==0x2f00))){
                const bool on=u8(bytes,at+4)==0xbc;action.kind="requirement";action.title=QString("%1 is %2").arg(EventScript::flagName(arg,row),on?"ON":"OFF");
                auto f=field(at,"value","Flag");f.choices.clear();for(int n=0;n<256;++n)f.choices[n]=EventScript::flagName(n,row);action.fields<<f;
                if(flagControls.contains(at)){StoryField state{"State",{},at,on?1:0,0,1,{{0,"OFF"},{1,"ON"}}, {},StoryField::FlagValue};action.fields<<state;}
                consume(word(at+6)==0x2f00?4:6);
            }else if((event.group==1||event.group==2)&&word(at)==0x61f8&&word(at+2)>=0x6a00&&word(at+2)<=0x6aff&&
                word(at+4)==0xa601&&word(at+6)==0x2f00&&owns(at,4)){
                action.kind="condition";const int town=u8(bytes,at+3);
                action.title="Visited town is "+towns.value(town,QString("Town ID %1").arg(town));
                auto f=field(at+2,"value","Town");f.choices=towns;f.kind="scenario-site";action.fields<<f;
                action.detail="Otherwise this event does not run.";consume(4);
                if(event.actions.isEmpty()&&event.triggerFields.isEmpty()){event.triggerFields=action.fields;event.triggerInstructions=action.instructions;if(!original)event.title=action.title;at+=count*2;continue;}
            } else if(owns(at,3)&&op==0x68&&u8(bytes,at+2)==0x69&&u8(bytes,at+3)<=1&&word(at+4)==0x7160){
                action.kind="action";action.title=QString("%1 %2").arg(u8(bytes,at+3)?"Set":"Clear",EventScript::flagName(arg,row));
                auto f=field(at,"value","Flag");f.choices.clear();for(int n=0;n<256;++n)f.choices[n]=EventScript::flagName(n,row);action.fields<<f;
                auto state=field(at+2,"value","State");state.choices={{0,"OFF"},{1,"ON"}};action.fields<<state;consume(3);
            } else if(owns(at,3)&&op==0x69&&word(at+2)==0x6061&&(u8(bytes,at+4)==0xac||u8(bytes,at+4)==0xbc)){
                action.kind="condition";const bool on=u8(bytes,at+4)==0xbc;
                action.title=QString("%1 is %2").arg(EventScript::flagName(arg,row),on?"ON":"OFF");
                auto f=field(at,"value","Flag");f.choices.clear();for(int n=0;n<256;++n)f.choices[n]=EventScript::flagName(n,row);action.fields<<f;
                if(flagControls.contains(at)){StoryField state{"State",{},at,on?1:0,0,1,{{0,"OFF"},{1,"ON"}}, {},StoryField::FlagValue};action.fields<<state;}
                action.target=at+6+qint8(u8(bytes,at+5))*2;action.otherwise=at+6;consume(3);
            } else if(owns(at,4)&&op==0x69&&arg==3&&u8(bytes,at+2)==0x6a&&word(at+4)==0x6391&&conditional(u8(bytes,at+6))&&(u8(bytes,at+6)&15)==11){
                action.kind="condition";const int answer=u8(bytes,at+3);
                auto f=field(at+2,"value","Answer");f.kind="dialogue-answer";f.choices.clear();
                if(lastScene>=0&&originalSceneText&&!scenes.modified())for(const auto &v:catalog()["answers"].toArray()){
                    const auto a=v.toArray();if(a[0].toInt()==lastScene&&a[1].toInt()==3){const int id=a[2].toInt();const auto label=a[3].toString();
                        if(f.choices.contains(id)&&f.choices[id]!=label)f.choices[id]=QString("Answer ID %1 (multiple questions)").arg(id);else f.choices[id]=label;}}
                if(!f.choices.contains(answer))f.choices[answer]=QString("Answer ID %1").arg(answer);
                action.title=QString("Dialogue answer %1 %2").arg(relation(u8(bytes,at+6)),f.choices[answer]);action.fields<<f;
                action.target=at+8+qint8(u8(bytes,at+7))*2;action.otherwise=at+8;consume(4);
            } else if(op>=0x60&&op<=0x63&&arg>=0xe4&&owns(at,3)&&u8(bytes,at+2)>=0x68&&u8(bytes,at+2)<=0x6b&&
                conditional(u8(bytes,at+4))&&(u8(bytes,at+4)&15)<12&&
                ((u8(bytes,at+4)>>2)&3)==(op&3)&&(u8(bytes,at+4)&3)==(u8(bytes,at+2)&3)){
                action.kind="condition";const auto property=catalog()["properties"].toObject()[QString::number(arg)].toObject()["name"].toString(QString("Property ID %1").arg(arg));
                action.title=QString("%1 %2 %3").arg(property,relation(u8(bytes,at+4))).arg(u8(bytes,at+3));
                action.fields<<field(at+2,"value","Value");action.target=at+6+qint8(u8(bytes,at+5))*2;action.otherwise=at+6;consume(3);
            } else {
                const bool load=op>=0x68&&op<=0x6f;const int reg=op&7;
                const int literalWords=load&&owns(at,2)&&u8(bytes,at+2)==op+0x20?2:1;
                const int after=at+literalWords*2,value=arg|(literalWords==2?u8(bytes,at+3)<<8:0);
                if(load&&reg==0&&literalWords==1&&owns(at,2)&&(u8(bytes,after)==2||u8(bytes,after)==3)){
                    action.kind="route";action.title=u8(bytes,after)==3?"Run shared event logic":"Continue in shared event logic";
                    auto f=field(after,"destination","Destination");action.target=f.value;action.fields<<f;consume(2);lastScene=-1;
                }else if(load&&reg==0&&owns(at,literalWords+1)&&u8(bytes,after)==0x10){
                    action.kind="action";const auto site=launches.value(after);action.scene=site.id.isEmpty()?value:events.target(site.id);
                    action.title="Play cutscene · "+sceneNames.value(action.scene,QString("Cutscene ID %1").arg(action.scene));
                    auto f=field(at,"value","Cutscene");f.value=action.scene;f.maximum=site.id.isEmpty()?f.maximum:65535;f.choices=sceneNames;f.kind="scene";action.fields<<f;
                    if(u8(bytes,after+1))action.detail=QString("Additional launch options: %1. See Raw instructions.").arg(u8(bytes,after+1));
                    consume(literalWords+1);lastScene=action.scene;
                }else if(load&&reg==0&&owns(at,literalWords+1)&&u8(bytes,after)==0x16&&!(u8(bytes,after+1)>=5&&u8(bytes,after+1)<=8)){
                    const int mode=u8(bytes,after+1),kind=mode&63;const bool processed=owns(at,literalWords+2)&&word(after+2)==0x1316;
                    action.kind="action";
                    if(mode&128){action.title="Remove characters with identity · "+reference("identity-class",value);action.fields<<field(at,"value","Identity","identity-class");}
                    else if(kind<=1){action.title=QString("%1 %2 · %3").arg(processed?"Recruit":"Queue recruitment of",kind==0?"squad":"character",reference("recruitment-template",value));action.fields<<field(at,"value","Recruitment template","recruitment-template");}
                    else if(kind==2){action.title=(processed?"Recruit opposing squad containing · ":"Queue opposing squad recruitment · ")+reference("identity-class",value);action.fields<<field(at,"value","Identity","identity-class");}
                    else{action.title=QString("Army action ID %1").arg(mode);action.fields<<field(at,"value","Argument");}
                    if(!(mode&128)){
                        auto modeField=field(after,"operand","Recruit as");modeField.mask=63;modeField.value=kind;modeField.maximum=63;modeField.kind="named-state";
                        modeField.choices={{0,"Squad and characters"},{1,"Characters in reserve"},{2,"Import opposing squad"}};
                        if(!modeField.choices.contains(kind))modeField.choices[kind]=QString("Characters in reserve (native mode %1)").arg(kind);action.fields<<modeField;
                        auto oath=field(after,"operand","Oath-based stat adjustments");oath.mask=64;oath.shift=6;oath.value=(mode>>6)&1;oath.maximum=1;oath.kind="named-state";oath.choices={{0,"OFF"},{1,"ON"}};action.fields<<oath;
                    }
                    consume(literalWords+1+(processed?1:0));
                }else if(load&&owns(at,literalWords+1)&&u8(bytes,after)==0x70+reg&&u8(bytes,after+1)>=0xe4){
                    const int property=u8(bytes,after+1);action.kind="action";
                    const auto label=catalog()["properties"].toObject()[QString::number(property)].toObject()["name"].toString(QString("Property ID %1").arg(property));
                    action.title=QString("Set %1 to %2").arg(label).arg(value);auto f=field(at,"value","Value");
                    if(property==0xe9||property==0xe7){f.choices=names("scenario");f.label="Scenario";action.title="Select scenario · "+reference("scenario",value);}
                    action.fields<<f;consume(literalWords+1);
                    if(property==0xe9&&owns(at,count+1)&&word(at+count*2)==0x70e7){action.instructions<<at+count*2;++count;}
                }else if(load&&reg==0&&owns(at,literalWords+1)&&u8(bytes,after)==0x11){
                    const int token=(u8(bytes,after+1)<<8)|(value&255);action.kind="action";
                    action.title=token==1?"Finish this event":token==0?"Return to gameplay":"Request · "+requestName(token&255);
                    auto f=field(at,"value","Next operation");f.choices=requestChoices();if(u8(bytes,after+1)==0)f.choices[1]="Finish this event";
                    f.mask=255;f.value&=255;f.maximum=255;action.fields<<f;consume(literalWords+1);lastScene=-1;
                }else if(load&&reg==0&&owns(at,literalWords+1)&&u8(bytes,after)==0x12){
                    action.kind="program";action.title="Load event program · "+EventScript::rowName(u8(bytes,after+1));
                    action.fields<<field(after,"operand","Event program");auto first=field(at,"value","Clear flags from");first.mask=255;first.value&=255;first.maximum=255;action.fields<<first;
                    if(literalWords==2)action.fields<<field(at+2,"value","Clear flags before (exclusive)");
                    action.detail="The selected flag range is cleared before changing event program.";consume(literalWords+1);lastScene=-1;
                }else if(op==0x16&&arg>=5&&arg<=8){action.kind="action";action.title=QString("Add starting squad %1 from the saved opening choices").arg(arg-4);action.fields<<field(at,"operand","Starting squad operation");consume(1);
                }else if(op==0x18){action.kind="action";action.title="Request world-map travel destination";auto f=field(at,"operand","Map location","world-location");f.choices[0]="No forced destination";action.fields<<f;action.detail="Used when the world map next processes travel.";consume(1);
                }else if(op==0x13){action.kind="action";action.title=requestName(arg);auto f=field(at,"operand","Operation");f.choices=requestChoices();action.fields<<f;consume(1);lastScene=-1;
                }else if(op==4){action.kind="return";action.title="Return to the calling event logic";consume(1);lastScene=-1;
                }else if(op==0x2f){action.kind="end";
                    // Row 0 is the arrival check, called by the world-map mode.
                    // A zero result leaves that mode active; investigation is a
                    // separate request through row 70, not a fall-through here.
                    action.title=row==0&&entry["group"]=="world-location"?"Stay on the world map":"Finish this event";
                    consume(1);lastScene=-1;
                }else if(op==1){action.kind="route";action.title="Continue at another step";action.target=at+2+qint8(arg)*2;action.fields<<field(at,"destination","Destination");consume(1);lastScene=-1;
                }else if((op==2||op==3)&&at>=2&&word(at-2)>=0x6800&&word(at-2)<=0x68ff){
                    action.kind="route";action.title=op==3?"Run shared event logic":"Continue in shared event logic";action.target=quint16((at+2)/2+(arg<<8)+u8(bytes,at-1))*2;action.fields<<field(at,"destination","Destination");consume(1);lastScene=-1;
                }else {
                    action.kind="advanced";action.advanced=true;action.title="Additional game logic";consume(1);
                    // Details remain available in Raw; no inferred story meaning
                    // is substituted for an unrecognized computation or branch.
                    if(conditional(op)||op==0x2d){action.target=at+2+qint8(arg)*2;action.otherwise=at+2;lastScene=-1;}
                }
            }
            if(action.instructions.isEmpty())consume(count);
            if(action.kind=="requirement"&&event.actions.isEmpty())event.requirements<<action;
            else if(action.advanced&&!event.actions.isEmpty()&&event.actions.last().advanced&&
                !entrances.contains(at)&&event.actions.last().target<0&&
                std::all_of(event.actions.last().instructions.begin(),event.actions.last().instructions.end(),[&](int pc){return pureProducer(u8(bytes,pc));})){
                auto &previous=event.actions.last();previous.instructions+=action.instructions;previous.target=action.target;previous.otherwise=action.otherwise;
            }
            else event.actions<<action;
            at+=count*2;
        }
        // Resolve native register plumbing into its consumer. Each expression
        // is propagated through the current control-flow graph, with conflicting
        // incoming definitions becoming unknown rather than borrowing a label
        // from one retail path. Source fields still edit their actual producers.
        for(auto &action:event.actions){
            if(action.instructions.isEmpty())continue;
            const int branch=action.instructions.last(),op=u8(bytes,branch),arg=u8(bytes,branch+1);
            bool pure=true;
            for(int pc:action.instructions)if(pc!=branch)pure&=pureProducer(u8(bytes,pc));
            const bool dialogueAnswer=std::any_of(action.fields.begin(),action.fields.end(),[](const auto &f){return f.kind=="dialogue-answer";});
            if(dialogueAnswer)action.fields<<field(branch,"comparison","Comparison");
            if(pure&&conditional(op)&&inputs.contains(branch)&&!dialogueAnswer){
                const auto &r=inputs[branch];const bool single=(op&15)>=12;
                Expression zero{"0","literal",0};
                const auto &left=r[single?(op&3):((op>>2)&3)];const auto &right=single?zero:r[op&3];
                auto condition=comparisonCondition(left,right,op);
                if(!condition.operation.isEmpty()&&(op>>4==10||op>>4==11)&&
                    (event.group==3||event.group==4)&&keys.size()==1){
                    const Expression *subject=&left,*literal=&right;
                    if(literal->kind=="property"&&literal->property==0xfd)std::swap(subject,literal);
                    if(subject->kind=="property"&&subject->property==0xfd&&literal->kind=="literal"&&literal->direct&&
                        literal->fields.size()==1&&literal->fields.first().key=="value"){
                        const auto squadNames=names("scenario-squad",QString("scenario:%1").arg(keys.first().toInt()));
                        const auto name=squadNames.value(literal->value,QString("Squad ID %1").arg(literal->value));
                        condition.title=QString("%1 %2 %3 [%4]").arg(subject->text,op>>4==10?"is":"is not",name).arg(literal->value);
                        for(auto &f:condition.fields)if(f.offset==literal->fields.first().offset&&f.key=="value"){
                            f.label="Squad";f.kind="scenario-squad";f.choices.clear();
                            for(int id=f.minimum;id<=qMin(f.maximum,255);++id)f.choices[id]=squadNames.value(id,QString("Squad ID %1").arg(id));
                            if(!f.choices.contains(f.value))f.choices[f.value]=QString("Squad ID %1").arg(f.value);
                        }
                    }
                }
                if(!condition.operation.isEmpty()){
                    auto comparison=field(branch,"comparison","Comparison");comparison.kind="named-state";
                    comparison.choices={{10,"Equals"},{11,"Does not equal"},{12,"Greater than"},{13,"Less than"},{14,"At least"},{15,"At most"}};
                    if(left.kind=="flag"&&right.kind=="literal"&&(right.value==0||right.value==1)&&(op>>4==10||op>>4==11)){
                        comparison.label="State";comparison.mode=StoryField::BooleanComparison;comparison.shift=(left.value<0?1:0)^right.value;
                        comparison.minimum=0;comparison.maximum=1;comparison.value=(op>>4==11?1:0)^comparison.shift;comparison.choices={{0,"OFF"},{1,"ON"}};
                        condition.title=left.text+" is "+comparison.choices[comparison.value];condition.fields=left.fields;
                    }else if(std::any_of(condition.fields.begin(),condition.fields.end(),[](const auto &f){return f.mode==StoryField::SquadState;})){
                        comparison.label="Match";comparison.choices={{10,"Matches both choices"},{11,"Does not match both choices"}};
                    }
                    condition.fields<<comparison;
                    action.kind="condition";action.advanced=false;action.title=condition.title;action.condition=condition;
                    action.fields.clear();action.target=branch+2+qint8(arg)*2;action.otherwise=branch+2;
                }
            }else if(pure&&op==0x2d&&inputs.contains(branch)&&!inputs[branch][0].text.isEmpty()){
                action.condition={"test","Army Goth < "+inputs[branch][0].text};action.condition.fields=inputs[branch][0].fields;
                action.kind="condition";action.advanced=false;action.title=action.condition.title;action.fields.clear();
                action.target=branch+2+qint8(arg)*2;action.otherwise=branch+2;
            }
            if(action.advanced&&pure&&inputs.contains(branch)&&((op>=0x70&&op<=0x77)||(op>=0x80&&op<=0x87))){
                const auto &r=inputs[branch];const auto &value=r[op&7];const auto &index=r[arg&7];const bool alternate=op>=0x80;
                const auto definitions=catalog()["banks"].toObject()[alternate?"altSetters":"setters"].toArray();
                const int bank=arg>>3;
                if(!value.text.isEmpty()&&!index.text.isEmpty()&&bank<definitions.size()&&definitions[bank].toObject()["status"]=="verified"){
                    action.kind="action";action.advanced=false;action.title="Set "+definitions[bank].toObject()["name"].toString()+" · "+index.text+" to "+value.text;
                    action.fields=value.fields;appendFields(action.fields,index.fields);
                    if(!alternate&&bank==12&&index.kind=="literal"&&index.value<256){
                        const auto flagName=EventScript::flagName(index.value,row);
                        action.title=value.kind=="literal"?QString("%1 %2").arg(value.value==1?"Set":"Clear",flagName):QString("Set %1 if %2 = 1; otherwise clear it").arg(flagName,value.text);
                        if(index.direct&&index.fields.size()==1)for(auto &f:action.fields)if(f.offset==index.fields.first().offset){f.label="Flag";f.choices.clear();for(int n=0;n<256&&n<=f.maximum;++n)f.choices[n]=EventScript::flagName(n,row);}
                    }
                }
            }
            if(action.advanced&&pure&&op==0x15&&inputs.contains(branch)){
                const auto &r=inputs[branch];if(!r[0].text.isEmpty()&&!r[1].text.isEmpty()&&!r[2].text.isEmpty()){
                    action.kind="action";action.advanced=false;action.title="Record history · "+reference("scene-history",arg);
                    action.fields<<field(branch,"operand","History entry","scene-history");
                    for(int n=0;n<3;++n)appendFields(action.fields,r[n].fields);
                }
            }
            if(action.advanced&&pure&&(op==0x17||op==0x2e)&&inputs.contains(branch)&&inputs[branch][0].kind=="literal"){
                const int amount=qint16((arg<<8)|(inputs[branch][0].value&255));
                action.kind="action";action.advanced=false;
                action.title=op==0x17?QString("Advance the calendar by %1 days").arg(amount):QString("%1 %2 Goth").arg(amount<0?"Give the army":"Take from the army").arg(qAbs(amount));
                action.fields<<field(branch,"signedAmount",op==0x17?"Days (negative = earlier)":"Goth to subtract (negative = give)");
            }
            if(action.advanced&&pure&&(op==0x1e||op==0x22)&&inputs.contains(branch)&&inputs[branch][0].kind=="literal"&&inputs[branch][0].direct&&
                inputs[branch][0].fields.size()==1&&inputs[branch][0].fields.first().key=="value"){
                const auto &value=inputs[branch][0];action.kind="action";action.advanced=false;
                if(op==0x1e){action.title=arg?"Select biography for · "+reference("report-person",value.value&255):"Hide from the People board · "+reference("report-person",value.value&255);
                    auto identity=value.fields.value(0);identity.label="Character identity";identity.value=value.value&255;identity.maximum=255;identity.mask=255;identity.kind="identity-class";identity.choices=names("identity-class");action.fields<<identity;
                    action.fields<<field(branch,"operand","Biography variant");
                }else{
                    action.title="Change story character classes · "+reference("identity-class",arg);action.fields<<field(branch,"operand","Identity to replace","identity-class");
                    auto identity=value.fields.value(0);identity.label="New identity class";identity.value=(value.value>>8)&255;identity.minimum=0;identity.maximum=255;identity.mask=65280;identity.shift=8;identity.kind="identity-class";identity.choices=names("identity-class");
                    auto current=identity;current.label="New current class";current.value=value.value&255;current.mask=255;current.shift=0;
                    if(value.fields.first().maximum>255)action.fields<<identity;action.fields<<current;
                }
            }
            if(action.advanced&&pure&&(op==0x1f||op==0x20||op==0x21||op==0x29)){
                action.kind="action";action.advanced=false;
                if(op==0x1f){action.title="Select People relationship board · "+reference("relationship-board",arg);action.fields<<field(branch,"operand","Relationship board","relationship-board");}
                else if(op==0x20)action.title="Initialize the first legion";
                else if(op==0x21){action.title=arg?"Enable the Legion-count menu":"Disable the Legion-count menu and clear movement groups";action.fields<<field(branch,"operand","Legion menu mode");}
                else action.title="Remember the current world map location";
            }
            if((action.kind=="condition"||action.kind=="requirement")&&action.condition.operation.isEmpty()){
                action.condition={"test",action.title};action.condition.fields=action.fields;action.fields.clear();
            }
        }
        for(auto &requirement:event.requirements)if(requirement.condition.operation.isEmpty()){requirement.condition={"test",requirement.title};requirement.condition.fields=requirement.fields;requirement.fields.clear();}
        // This exact case is a complete visit -> cutscene -> return event.
        // The return is represented by the event's implicit completion, not lost.
        if(!event.triggerFields.isEmpty()&&event.actions.size()==2&&event.actions[0].scene>=0&&event.actions[1].title=="Return to gameplay"&&
            !entrances.contains(event.actions[1].instructions.first())){
            event.simpleVisit=true;event.actions[0].instructions+=event.actions[1].instructions;event.actions.removeLast();
            event.title=event.triggerFields.first().choices.value(event.triggerFields.first().value,QString("Town ID %1").arg(event.triggerFields.first().value));
        }
        // The first exact-target guard is part of WHEN, not an unrelated IF.
        const int guard=event.entry;
        if(event.group>=1&&event.group<=4&&event.triggerFields.isEmpty()&&event.actions.size()>=2&&
           guard+8<=bytes.size()&&u8(bytes,guard)==0x61&&(u8(bytes,guard+1)==0xfd||u8(bytes,guard+1)==0xf8)&&
           u8(bytes,guard+2)==0x6a&&u16(bytes,guard+4)==0xa601&&u16(bytes,guard+6)==0x2f00&&
           event.actions[0].instructions==QVector<int>{guard,guard+2,guard+4}&&event.actions[1].instructions==QVector<int>{guard+6}){
            auto f=field(guard+2,"value",event.group<=2?"Town":"Squad");f.kind=event.group<=2?"scenario-site":"scenario-squad";f.choices=event.group<=2?towns:names("scenario-squad",keys.size()==1?QString("scenario:%1").arg(keys.first().toInt()):QString{});
            event.triggerFields<<f;event.triggerInstructions={guard,guard+2,guard+4,guard+6};event.actions.remove(0,2);
        }
        const auto changedTrigger=triggerChanges[QString::number(event.entry)].toObject();
        if(!changedTrigger.isEmpty()&&changedTrigger["target"].toInt(-1)<0){const int disabled=changedTrigger["guard"].toInt(-1);
            if(disabled>=0)for(int i=0;i+1<event.actions.size();++i)if(event.actions[i].instructions==QVector<int>{disabled,disabled+2,disabled+4}&&event.actions[i+1].instructions==QVector<int>{disabled+6}){
                event.triggerInstructions={disabled,disabled+2,disabled+4,disabled+6};event.actions.remove(i,2);break;}}
        if(!event.actions.isEmpty()||!event.triggerInstructions.isEmpty()||!event.requirements.isEmpty())result<<event;
    }
    // Authored arrival rules are saved structured data. Their generated prefix
    // is presented from that data and edited through setWorldRules; anonymous
    // appended byte offsets are never persisted as ordinary retail operands.
    if(row==0){
        const auto edited=events.flagEdits()["0"].toObject();
        for(auto it=edited.begin();it!=edited.end();++it){bool ok=false;const int node=it.key().toInt(&ok);
            if(!ok||node<1||node>40||!it.value().isArray())continue;
            const auto rules=it.value().toArray();if(rules.isEmpty())continue;
            int at=u16(bytes,node*2);StoryEvent event;event.entry=at;event.trigger="Arrive at map location";
            event.title="Arrival · "+reference("world-location",node);
            auto ruleField=[&](int rule,int condition,int offset,QString key,QString label,int value,int maximum=255){
                StoryField f{label,key,offset,value,0,maximum};f.mode=StoryField::WorldRule;f.location=node;f.rule=rule;f.condition=condition;return f;
            };
            for(int n=0;n<rules.size();++n){const auto rule=rules[n].toObject();const auto conditions=rule["conditions"].toArray();
                const int actionAt=at+conditions.size()*6,next=actionAt+6;
                if(!conditions.isEmpty()){
                    StoryAction action;action.kind="condition";action.condition.operation="all";action.condition.title="All of these conditions";
                    for(int c=0;c<conditions.size();++c){const auto condition=conditions[c].toObject();const int pc=at+c*6,flag=condition["flag"].toInt();const bool on=condition["value"].toBool();
                        StoryCondition test{"test",QString("%1 is %2").arg(EventScript::flagName(flag,row),on?"ON":"OFF")};
                        auto f=ruleField(n,c,pc,"flag","Flag",flag);f.kind="flag";for(int id=0;id<256;++id)f.choices[id]=EventScript::flagName(id,row);test.fields<<f;
                        auto state=ruleField(n,c,pc,"state","State",on?1:0,1);state.choices={{0,"OFF"},{1,"ON"}};test.fields<<state;
                        action.condition.children<<test;action.instructions<<pc<<pc+2<<pc+4;
                    }
                    if(action.condition.children.size()==1)action.condition=action.condition.children.first();
                    action.title=action.condition.title;action.target=actionAt;action.otherwise=next;event.actions<<action;
                }
                StoryAction load;load.kind="program";load.title="Load event program · "+EventScript::rowName(rule["event"].toInt());load.instructions={actionAt,actionAt+2,actionAt+4};
                auto destination=ruleField(n,-1,actionAt+4,"event","Event program",rule["event"].toInt(),114);destination.minimum=1;
                for(int id=1;id<115;++id)destination.choices[id]=EventScript::rowName(id);load.fields<<destination;
                load.fields<<ruleField(n,-1,actionAt,"clearFrom","Clear flags from",rule["clearFrom"].toInt());
                load.fields<<ruleField(n,-1,actionAt+2,"clearTo","Clear flags before (exclusive)",rule["clearTo"].toInt());
                event.actions<<load;at=next;
            }
            StoryAction fallback;const int previous=u16(retail,node*2);
            if(previous){fallback.kind="route";fallback.title="Continue with existing arrival events";fallback.target=previous;fallback.instructions={at,at+2};}
            else {fallback.kind="end";fallback.title="Stay on the world map";fallback.instructions={at};}
            event.actions<<fallback;result<<event;
        }
    }
    const auto rowEdits=events.flagEdits()[QString::number(row)].toObject();const auto authored=rowEdits["trees"].toArray();
    if(!authored.isEmpty()){
        auto baseEdits=rowEdits;baseEdits.remove("trees");baseEdits.remove("triggers");auto flags=events.flagEdits();flags[QString::number(row)]=baseEdits;
        const auto base=SceneEvents::flagRow(retail,row,flags);const auto positions=EventAuthoring::entries(base,row,authored,triggerChanges);
        for(const auto &v:authored){const auto tree=v.toObject();StoryEvent e;e.entry=positions.value(tree["id"].toString());e.group=tree["group"].toInt();e.title=tree["name"].toString();e.trigger=trigger(e.group);e.authoredId=tree["id"].toString();result<<e;}
    }
    std::sort(result.begin(),result.end(),[](const auto &a,const auto &b){return a.entry<b.entry;});
    return result;
}
void EventFlow::set(SceneEvents &events,const ScenePrograms &scenes,int row,const StoryField &field,int value){
    if(value<field.minimum||value>field.maximum||(!field.choices.isEmpty()&&!field.choices.contains(value)))throw Error("Choose a supported event value");
    if(field.mode==StoryField::Operand&&field.key=="value"){
        const auto changes=events.flagEdits()[QString::number(row)].toObject()["triggers"].toObject();
        for(auto it=changes.begin();it!=changes.end();++it){auto trigger=it.value().toObject();if(trigger["guard"].toInt(-1)+2==field.offset){trigger["target"]=value;events.setTrigger(row,it.key().toInt(),trigger,scenes);return;}}
    }
    if(field.mode==StoryField::BooleanComparison){events.editParameter(row,field.offset,"comparison",(value!=field.shift)?11:10,scenes);return;}
    if(field.mode==StoryField::VisitorCheck){auto next=events;for(int at:{field.offset,field.location}){
        if(value>=0)next.editParameter(row,at,"comparison",value==1?10:11,scenes);
        next.editParameter(row,at,"destination",value<0?at+2:field.rule,scenes);}
        events=std::move(next);return;
    }
    if(field.mode==StoryField::SquadState){
        auto next=events;const auto bytes=events.program(row);int mask=-1,wanted=-1;
        for(const auto &p:EventScript::parameters(bytes,row,field.offset))if(p.key==field.key)mask=p.value;
        for(const auto &p:EventScript::parameters(bytes,row,field.location))if(p.key=="value")wanted=p.value;
        if(mask<0||wanted<0)throw Error("Squad state no longer has editable operands");
        mask=value<0?mask&~field.mask:mask|field.mask;wanted=value==1?wanted|field.mask:wanted&~field.mask;
        next.editParameter(row,field.offset,field.key,mask,scenes);next.editParameter(row,field.location,"value",wanted,scenes);events=std::move(next);return;
    }
    if(field.mode==StoryField::WorldRule){
        auto rules=events.flagEdits()["0"].toObject()[QString::number(field.location)].toArray();
        if(row!=0||field.rule<0||field.rule>=rules.size())throw Error("Arrival rule is no longer present");
        auto rule=rules[field.rule].toObject();
        if(field.condition>=0){auto conditions=rule["conditions"].toArray();if(field.condition>=conditions.size())throw Error("Arrival condition is no longer present");
            auto condition=conditions[field.condition].toObject();
            if(field.key=="flag")condition["flag"]=value;else if(field.key=="state")condition["value"]=value!=0;else throw Error("Unknown arrival condition field");
            conditions[field.condition]=condition;rule["conditions"]=conditions;
        }else if(field.key=="event"||field.key=="clearFrom"||field.key=="clearTo")rule[field.key]=value;
        else throw Error("Unknown arrival rule field");
        rules[field.rule]=rule;events.setWorldRules(field.location,rules);return;
    }
    if(field.mode==StoryField::FlagValue){const auto bytes=events.program(row);events.editFlag(row,field.offset,u8(bytes,field.offset+1),value!=0);return;}
    if(field.mask!=65535){const auto bytes=events.program(row);for(const auto &p:EventScript::parameters(bytes,row,field.offset))if(p.key==field.key)value=(p.value&~field.mask)|((value<<field.shift)&field.mask);}
    events.editParameter(row,field.offset,field.key,value,scenes);
}
}
