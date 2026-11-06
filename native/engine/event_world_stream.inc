// World reveal lists are chains of (location, action) pairs. FF closes a
// chain; another FF ends the list. Native consumer: z64 0x7D9CC..0x7DBFC.
// Track the parser phase through CURRENT control flow, so a changed branch or
// interior entry never causes a byte in the action slot to acquire a town name.
enum WorldStreamPhase { WorldLocation=0, WorldAction=1, WorldNext=2,
                        WorldEnded=3, WorldUnknown=4 };
int advanceWorldStreamPhase(int phase,int value) {
    if(phase==WorldLocation)return value==255?WorldEnded:WorldAction;
    if(phase==WorldAction)return WorldNext;
    if(phase==WorldNext)return value==255?WorldLocation:WorldAction;
    return phase;
}

QMap<int,int> worldStreamPhases(const QByteArray &bytes,const QSet<int> &offsets,
                               const QJsonArray &handlers,const QMap<int,Registers> &registers) {
    QMap<int,int> masks,result;QVector<int> pending;QSet<int> queued;
    bool present=false;for(int at:offsets)if(at+1<bytes.size() && u8(bytes,at)==0x1b){present=true;break;}
    if(!present)return result;
    auto merge=[&](int at,int mask){
        if(!offsets.contains(at) || at<0 || at+1>=bytes.size())return;
        const int joined=masks.value(at)|mask;
        if(joined==masks.value(at))return;
        masks[at]=joined;if(!queued.contains(at)){pending<<at;queued.insert(at);}
    };
    for(const auto &v:handlers)merge(v.toObject()["entry"].toInt(),1<<WorldUnknown);
    while(!pending.isEmpty()){
        const int at=pending.takeLast();queued.remove(at);
        const int op=u8(bytes,at),arg=u8(bytes,at+1);int after=masks[at];
        if(op==0x6f)after=1<<(arg==0?WorldLocation:WorldUnknown);
        else if(op==0x1b){
            after=0;for(int phase=0;phase<=WorldUnknown;++phase)
                if(masks[at]&(1<<phase))after|=1<<advanceWorldStreamPhase(phase,arg);
        }else if(op==0x67 || op==0x7f || op==0x8f || op==0x97 || op==0x9f ||
                 op==0x28 || (op==6 && (arg&7)==7) || op==0x10 || op==0x13)
            after=1<<WorldUnknown;
        if(op==1 || op==0x2d || conditional(op)){
            merge(at+2+qint8(arg)*2,after);
            if(op!=1)merge(at+2,after);
        }else if(op==2 || op==3){
            const auto r0=registers.value(at)[0];
            if(r0.kind=="literal")merge(quint16((at+2)/2+(arg<<8)+(r0.value&255))*2,after);
            // A returning local call may have appended arbitrary bytes.
            if(op==3)merge(at+2,1<<WorldUnknown);
        }else if(op!=4 && op!=0x12 && op!=0x2f && op!=0x11)merge(at+2,after);
    }
    for(auto it=masks.begin();it!=masks.end();++it){
        int phase=WorldUnknown;
        for(int n=0;n<=WorldUnknown;++n)if(it.value()==(1<<n)){phase=n;break;}
        result[it.key()]=phase;
    }
    return result;
}

struct WorldStreamGroup {
    int count=0;
    QString title,detail;
    QVector<StoryField> fields;
};

WorldStreamGroup groupWorldStream(const QByteArray &bytes,const QSet<int> &offsets,
                                  const QSet<int> &entrances,const QMap<int,int> &phases,
                                  int at,int end) {
    WorldStreamGroup group;int first=at;
    const auto valid=[&](int pc){return pc>=at && pc<end && pc+1<bytes.size() && offsets.contains(pc) &&
                                     (pc==at || !entrances.contains(pc));};
    if(!valid(at))return group;
    int phase=phases.value(at,WorldUnknown);
    if(u8(bytes,at)==0x6f && valid(at+2) && u8(bytes,at+2)==0x1b){
        const int position=u8(bytes,at+1);
        phase=position==0?WorldLocation:WorldUnknown;first+=2;++group.count;
    }
    if(u8(bytes,first)!=0x1b)return {};
    int stop=first;
    while(valid(stop) && u8(bytes,stop)==0x1b){++group.count;stop+=2;}
    const QMap<int,QString> actions{{0,"Hide location"},{1,"Mark location cleared / normal"},
        {2,"Open location for a mission"},{3,"Open the next connecting road"},
        {4,"Close the next connecting road"},{5,"Clear active mission marker"},
        {6,"Set active mission marker"}};
    QMap<int,QString> locations;
    const auto known=names("world-location");
    for(auto it=known.begin();it!=known.end();++it)if(it.key()>=1 && it.key()<=40)locations[it.key()]=it.value();
    QStringList changes,namedLocations;QString currentLocation;
    bool fragment=phase==WorldUnknown || phase==WorldAction || phase==WorldEnded,roads=false;
    for(int pc=first;pc<stop;pc+=2){
        const int value=u8(bytes,pc+1);
        StoryField field{"Update list value","operand",pc,value,0,255};
        if(phase==WorldUnknown){
            continue;
        }
        if(phase==WorldEnded){
            fragment=true;continue;
        }
        if(phase==WorldAction){
            if(actions.contains(value) && !currentLocation.isEmpty()){
                field.label=currentLocation+" update";
                field.kind="named-state";field.choices=actions;group.fields<<field;
            }else fragment=true;
        }else if(value!=255){
            const auto name=locations.value(value,QString("World location ID %1 (unclassified)").arg(value));
            currentLocation=locations.contains(value) && pc+2<stop && u8(bytes,pc+3)<=6?name:QString{};
            if(!currentLocation.isEmpty()){
                field.label="World location";field.kind="world-location";field.choices=locations;group.fields<<field;
            }else fragment=true;
            if(!namedLocations.contains(name))namedLocations<<name;
            if(pc+2<stop){
                const int action=u8(bytes,pc+3);
                if(action<=2 || action==5 || action==6)changes<<actions.value(action)+" · "+name;
                else if(action>6)changes<<QString("Unclassified location update %1 · %2").arg(action).arg(name);
                if(pc+4<stop && u8(bytes,pc+5)!=255 && action<=6){
                    const int target=u8(bytes,pc+5);
                    const auto targetName=locations.value(target,QString("World location ID %1 (unclassified)").arg(target));
                    changes<<QString("%1 connecting road · %2 → %3")
                        .arg(action==0 || action==4?"Close":"Open",name,targetName);roads=true;
                }else if((action==3 || action==4) && pc+4<stop && u8(bytes,pc+5)==255)
                    changes<<"Finish road chain at "+name;
            }else {changes<<"Begin location update · "+name;fragment=true;}
        }
        if(value==255 && phase!=WorldAction)currentLocation.clear();
        phase=advanceWorldStreamPhase(phase,value);
    }
    if(namedLocations.isEmpty())group.title=fragment?"World map update fragment":"Finish world map update list";
    else group.title="Update world map · "+namedLocations.join(", ");
    if(!changes.isEmpty())group.detail=changes.join("\n");
    if(roads)group.detail+="\nRoad changes apply when the two locations have a connecting road.";
    if(fragment){
        if(!group.detail.isEmpty())group.detail+="\n";
        group.detail+="Some update context is unclassified. Inspect raw instructions for those values.";
    }
    return group;
}
