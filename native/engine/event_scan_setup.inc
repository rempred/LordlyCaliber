// Included inside event_flow.cpp's anonymous namespace, after Expression.
// Presentation of existing owned instructions only: no new grouping, hidden
// loop head, guessed register role, or assumption that a scan visits every slot.
namespace scan_setup {
struct Role {QString name,start,step;};
Role role(const Expression &e){
    if(e.kind=="member-index")return {"squad member","Starting member position (zero-based)","member position"};
    if(e.kind=="army-index")return {"army character","Starting army character slot","character slot"};
    if(e.kind=="opposing-index")return {"opposing character","Starting opposing character slot","character slot"};
    if(e.kind=="consumable-index")return {"consumable inventory","Starting consumable inventory slot","inventory slot"};
    if(e.kind=="equipment-index")return {"equipment inventory","Starting equipment inventory slot","inventory slot"};
    if(e.kind=="temporary-flag-index")return {"temporary flag","Starting temporary flag","flag"};
    if(e.kind=="world-location-index")return {"world location","Starting location counter","location"};
    if(e.kind=="world-road-index")return {"world road","Starting road counter","road"};
    return {};
}
void field(QVector<StoryField> &fields,int at,const QString &key,const QString &label,int value,int maximum){
    for(auto &f:fields)if(f.offset==at&&f.key==key){f.label=label;return;}
    fields<<StoryField{label,key,at,value,0,maximum};
}
}
bool describeScanStart(StoryAction &action,const QByteArray &bytes,const QMap<int,Registers> &iterators){
    if(action.instructions.isEmpty())return false;
    for(int at:action.instructions)if(at<0||at+1>=bytes.size())return false;
    auto fields=action.fields;
    QStringList clauses;
    // A unit stride advances the scan. The instruction itself is available in
    // Raw; a zero stride can stall the game instead of advancing the scan.
    bool increments=true;
    for(int at:action.instructions){
        const int op=u8(bytes,at),arg=u8(bytes,at+1);
        const auto index=iterators.value(at)[op&7];const auto role=scan_setup::role(index);
        if(op<0x98||op>0x9f||(arg>>5)!=2||(arg&31)!=1||role.name.isEmpty()){increments=false;break;}
        clauses<<"Advance "+role.name+" scan to the next "+role.step;
    }
    if(increments){action.title=clauses.join("; ");action.fields=fields;action.kind="action";action.advanced=false;return true;}
    fields=action.fields;clauses.clear();

    QMap<int,int> definitions;
    QMap<int,Expression> starts;
    QSet<int> explained;
    for(int at:action.instructions){
        const int op=u8(bytes,at);
        // Refuse mixed read/arithmetic/effect blocks. Describing just their last
        // literal would silently discard another instruction's purpose.
        if(op<0x68||op>0x6f||definitions.contains(op&7))return false;
        definitions[op&7]=at;
        const auto index=iterators.value(at+2)[op&7];
        if(index.property==at&&index.bank==1&&!scan_setup::role(index).name.isEmpty()){
            starts[op&7]=index;explained.insert(at);
        }
    }
    if(starts.isEmpty())return false;
    QMap<int,QString> cutoffLabels;
    // The must-analysis records actual reaching literal definitions used in
    // comparisons with this iterator. This also proves increment-first world
    // resets, whose branch comes after their write rather than after setup.
    for(auto start=starts.cbegin();start!=starts.cend();++start)for(const auto &bound:start.value().children){
        if(bound.kind!="scan-cutoff"||bound.value<0)continue;
        for(auto definition=definitions.cbegin();definition!=definitions.cend();++definition){
            const int at=definition.value();
            if(starts.contains(definition.key())||bound.property!=at||bound.value!=u8(bytes,at+1))continue;
            explained.insert(at);
            const QString label=start.value().kind=="member-index"?"Member position limit":
                start.value().kind=="army-index"?"Army character slot limit":
                start.value().kind=="opposing-index"?"Opposing character slot limit":
                start.value().kind=="consumable-index"?"Consumable slot limit":
                start.value().kind=="equipment-index"?"Equipment slot limit":
                start.value().kind=="temporary-flag-index"?"Temporary flag limit":
                start.value().kind=="world-location-index"?"Location scan comparison cutoff":"Road scan comparison cutoff";
            if(cutoffLabels.contains(at)&&cutoffLabels[at]!=label)cutoffLabels[at]="Scan comparison cutoff";
            else cutoffLabels[at]=label;
        }
    }
    if(explained.size()!=action.instructions.size())return false;
    for(int at:action.instructions){
        const int reg=u8(bytes,at)&7,value=u8(bytes,at+1);
        if(starts.contains(reg)){
            const auto &index=starts[reg];const auto role=scan_setup::role(index);
            if(index.kind=="member-index"){
                const QStringList positions{"first","second","third","fourth","fifth"};
                clauses<< (value<5?"Start squad member scan at the "+positions[value]+" position":
                    QString("Start squad member scan at custom position %1 (zero-based)").arg(value));
            }else if(index.kind=="world-location-index"||index.kind=="world-road-index")
                clauses<<QString("Initialize %1 scan counter to %2").arg(role.name).arg(value);
            else clauses<<QString("Start %1 scan at %2 %3").arg(role.name,role.step).arg(value);
            scan_setup::field(fields,at,"value",role.start,value,255);
            if(index.kind=="member-index")for(auto &f:fields)if(f.offset==at&&f.key=="value"){
                f.choices=memberPositions;f.kind="named-state";
                if(!f.choices.contains(value))f.choices[value]=QString("Custom position %1 (outside five squad positions)").arg(value);
            }
        }else{
            scan_setup::field(fields,at,"value",cutoffLabels.value(at,"Scan comparison cutoff"),value,255);
        }
    }
    action.title=clauses.join("; ");action.fields=fields;action.kind="action";action.advanced=false;
    return true;
}
