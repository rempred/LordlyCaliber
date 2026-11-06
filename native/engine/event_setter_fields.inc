// Included after Expression/appendFields in event_flow.cpp's anonymous namespace.
// Source fields always edit their actual producer. A domain is attached only to
// a direct literal, never to an arbitrary input of a folded calculation.
namespace setter_fields {
bool direct(const Expression &e) {
    return e.kind=="literal" && e.direct && e.fields.size()==1 &&
           e.fields.first().key=="value" && e.fields.first().value==e.value;
}
bool source(const StoryField &f,const Expression &e) {
    return direct(e) && f.offset==e.fields.first().offset &&
           f.key==e.fields.first().key && f.mode==e.fields.first().mode;
}
bool shared(const Expression &a,const Expression &b) {
    return direct(a) && direct(b) && source(a.fields.first(),b);
}
QString name(const QString &kind,int id,const QString &fallback) {
    return names(kind).value(id,QString("%1 %2").arg(fallback).arg(id));
}
QString number(const Expression &e,bool byte=false) {
    if(e.kind=="literal")return QString::number(byte?e.value&255:e.value);
    return byte&&!e.byte?"Low byte of "+e.text:e.text;
}
QString indexed(const Expression &e,const QString &kind,const QString &fallback,bool byte=false) {
    return e.kind=="literal"?name(kind,byte?e.value&255:e.value,fallback):number(e,byte);
}
void label(StoryAction &action,const Expression &e,const QString &label,
           const QString &kind={},QMap<int,QString> choices={},bool byte=false) {
    if(!direct(e))return;
    for(auto &f:action.fields)if(source(f,e)){
        f.label=label;f.kind=kind;f.choices.clear();
        if(byte){
            // This is a LOW-BYTE consumer, not a bit-field patch. Keep the
            // producer's unused upper byte intact when changing its value.
            f.mask=255;f.shift=0;f.value=e.value&255;
            f.minimum=0;f.maximum=qMin(255,f.maximum);
        }
        for(auto it=choices.begin();it!=choices.end();++it)
            if(it.key()>=f.minimum && it.key()<=f.maximum)f.choices[it.key()]=it.value();
        if(!f.choices.isEmpty() && !f.choices.contains(f.value))
            f.choices[f.value]=QString("Existing value %1").arg(f.value);
    }
}
void calculation(StoryAction &action,const Expression &e,const QString &quantity,const Expression &index) {
    if(e.kind!="calculation" || e.children.size()!=2 || e.value<0 || e.value>6)return;
    const auto &rhs=e.children[1];
    if(rhs.kind!="literal" || rhs.fields.size()!=1 || rhs.fields.first().value!=rhs.value ||
       (!rhs.direct && rhs.fields.first().key!="argument"))return;
    for(const auto &f:index.fields)if(f.offset==rhs.fields.first().offset && f.key==rhs.fields.first().key)return;
    const QStringList roles{"bit mask","bit flags","increment","decrement","multiplier","divisor","remainder divisor"};
    for(auto &f:action.fields)if(f.offset==rhs.fields.first().offset && f.key==rhs.fields.first().key)
        f.label=quantity+" "+roles[e.value];
}
QString inventory(const Expression &index,bool consumable) {
    const auto expected=consumable?QString("consumable-slot"):QString("equipment-slot");
    if(index.kind==expected && index.children.size()==1)
        return indexed(index.children.first(),consumable?"consumable":"equipment",consumable?"consumable":"equipment");
    return QString("%1 inventory slot %2").arg(consumable?"consumable":"equipment",number(index));
}
QMap<int,QString> worldLocations() {
    auto result=names("world-location");
    for(auto it=result.begin();it!=result.end();)
        if(it.key()<1 || it.key()>40)it=result.erase(it);else ++it;
    return result;
}
QString location(const Expression &index) {
    return index.kind=="literal"?worldLocations().value(index.value&255,
        QString("native location selector %1").arg(index.value&255)):number(index,true);
}
void squadFlags(StoryAction &action,const Expression &value) {
    if(!direct(value))return;
    QVector<StoryField> result;
    for(const auto &f:action.fields){
        if(!source(f,value)){result<<f;continue;}
        auto formation=f;formation.label="Squad formation";formation.kind="named-state";
        formation.mask=1;formation.shift=0;formation.value=value.value&1;
        formation.minimum=0;formation.maximum=1;formation.choices={{0,"Absent"},{1,"Present"}};
        auto side=f;side.label="Squad side";side.kind="named-state";
        side.mask=128;side.shift=7;side.value=(value.value>>7)&1;
        side.minimum=0;side.maximum=1;side.choices={{0,"Friendly"},{1,"Hostile"}};
        // These masks change the source byte; native service13 still replaces
        // its entire flags byte. Unnamed source bits remain untouched.
        result<<formation<<side;
    }
    action.fields=std::move(result);
}
} // namespace setter_fields

void describeSetter(StoryAction &action,const Expression &value,const Expression &index,
                    bool alternate,int bank) {
    using namespace setter_fields;
    const bool same=shared(value,index);
    const auto sharedLabel=[&](const QString &label){
        if(!same)return;
        for(auto &f:action.fields)if(source(f,value)){
            f.label=label+" (shared source)";f.kind.clear();f.choices.clear();
        }
    };
    const auto selectIndex=[&](const QString &labelText,const QString &kind={},QMap<int,QString> choices={},bool byte=false){
        if(!same)label(action,index,labelText,kind,std::move(choices),byte);
    };
    const auto selectValue=[&](const QString &labelText,const QString &kind={},QMap<int,QString> choices={},bool byte=false){
        if(!same)label(action,value,labelText,kind,std::move(choices),byte);
    };
    const auto booleanChoices=[&](bool exactOne){
        QMap<int,QString> choices{{0,"OFF"},{1,"ON"}};
        if(value.kind=="literal" && value.value!=0 && value.value!=1)
            choices[value.value]=QString("%1 (existing value %2)").arg(exactOne?"OFF":"ON").arg(value.value);
        return choices;
    };
    // Keep the existing persistent-flag title and contextual flag picker. Its
    // exact-one setter differs from the transient bank's nonzero setter.
    if(!alternate && bank==12){
        selectValue("Flag state","named-state",booleanChoices(true));
        sharedLabel("Flag number / state");
        return;
    }
    if(!alternate && (bank==24 || bank==25)){
        const auto actor=number(index),klass=indexed(value,"identity-class","class",true);
        action.title=QString("Set scene actor %1 %2 class · %3").arg(actor,bank==24?"identity":"current",klass);
        selectIndex("Scene actor slot","named-state",{{0,"First actor"},{1,"Second actor"},{2,"Third actor"},{3,"Fourth actor"},{4,"Fifth actor"}});
        selectValue(bank==24?"Identity class":"Current class","identity-class",names("identity-class"),true);
        sharedLabel("Scene actor slot / class");
        return;
    }
    if(!alternate && bank==22){
        const auto destination=location(index);
        const QMap<int,QString> states{{0,"Hidden"},{1,"Cleared / normal"},{2,"Open for a mission"}};
        const auto state=value.kind=="literal"?states.value(value.value&255,QString("native state value %1").arg(value.value&255)):number(value,true);
        action.title="Set world location · "+destination+" · "+state;
        selectIndex("World location","world-location",worldLocations(),true);
        // Native code truncates to a byte, clears two destination bits and ORs
        // the ENTIRE shifted byte. Masking this source to two bits would change
        // existing nonstandard writes and their effects on adjacent entries.
        selectValue("Location state","named-state",states,true);
        sharedLabel("World location / state");
        return;
    }
    if((!alternate && bank==23) || (alternate && bank==10)){
        const bool road=!alternate;
        const auto target=road?indexed(index,"world-road","world road",true):location(index);
        const int raw=value.kind=="literal"?value.value&255:-1;
        action.title=raw==0?(road?"Close world road · ":"Clear active mission marker · ")+target:
            raw==1?(road?"Open world road · ":"Set active mission marker · ")+target:
                QString("Write %1 value %2 · %3").arg(road?"world-road":"mission-marker",number(value,true),target);
        selectIndex(road?"World road":"World location",road?"world-road":"world-location",road?names("world-road"):worldLocations(),true);
        // As above, this consumer does not clamp the supplied byte to one bit.
        // Canonical OFF/ON choices replace the full low byte; preserve a current
        // nonstandard byte as its own choice instead of hiding its extra bits.
        selectValue(road?"Road state":"Mission marker","named-state",{{0,road?"Closed":"Inactive"},{1,road?"Open":"Active"}},true);
        sharedLabel(road?"World road / state":"World location / marker state");
        return;
    }
    if(!alternate && bank==13){
        action.title=value.kind=="literal"?QString("Set squad %1 · formation %2 · %3 side").arg(number(index),value.value&1?"present":"absent",value.value&128?"hostile":"friendly"):
            "Set squad "+number(index)+" saved formation / side flags";
        selectIndex("Squad");
        if(!same)squadFlags(action,value);
        sharedLabel("Squad / saved flags");
        return;
    }
    if(!alternate && bank==27){
        const auto target=indexed(index,"transient-game-state","temporary gameplay flag");
        action.title=value.kind=="literal"?(value.value?"Set ":"Clear ")+target:
            "Set "+target+" when "+value.text+" is nonzero; otherwise clear it";
        selectIndex("Temporary gameplay flag");
        selectValue("Flag state","named-state",booleanChoices(false));
        sharedLabel("Temporary flag number / state");
        return;
    }
    if(!alternate && bank==26){
        const bool reward=index.kind=="literal" && index.value==255;
        action.title=reward?"Set reward-panel item · "+indexed(value,"reward-display","reward token"):
            "Set cutscene text substitution "+number(index)+" to "+number(value);
        selectIndex("Text substitution slot");
        selectValue(reward?"Reward-panel item":"Replacement value",reward?"reward-display":"",reward?names("reward-display"):QMap<int,QString>{});
        if(value.kind=="service" && (value.bank==24 || value.bank==25) && value.children.size()==1){
            const auto &actor=value.children.first();
            const QMap<int,QString> actorSlots{{0,"First actor"},{1,"Second actor"},{2,"Third actor"},{3,"Fourth actor"},{4,"Fifth actor"}};
            const auto who=actor.kind=="literal"?actorSlots.value(actor.value,QString("scene actor slot %1").arg(actor.value)):actor.text;
            action.title=reward?QString("Set reward-panel token from %1's %2 class").arg(who.toLower(),value.bank==24?"identity":"current"):
                QString("Set cutscene text substitution %1 from %2's %3 class").arg(number(index),who.toLower(),value.bank==24?"identity":"current");
            if(shared(actor,index)){
                for(auto &f:action.fields)if(source(f,actor)){
                    f.label="Text substitution / source actor slot (shared source)";f.kind.clear();f.choices.clear();
                }
            }else label(action,actor,"Source scene actor slot","named-state",actorSlots);
        }
        sharedLabel("Text substitution slot / value");
        return;
    }
    if(alternate && bank>=2 && bank<=8 && bank!=5){
        const bool consumable=bank<=4,owned=bank==2 || bank==6,assigned=bank==4 || bank==8;
        const auto target=inventory(index,consumable);
        selectIndex(consumable?"Consumable inventory slot":"Equipment inventory slot");
        if(owned || assigned){
            const auto role=owned?QString("Owned copies"):consumable?QString("Carried copies"):QString("Equipped copies");
            action.title="Set "+role.toLower()+" of "+target+" to "+number(value,true);
            selectValue(role,{}, {},true);calculation(action,value,role,index);
        }else{
            action.title="Set item in "+target+" · "+indexed(value,consumable?"consumable":"equipment",consumable?"consumable":"equipment");
            selectValue(consumable?"Consumable":"Equipment",consumable?"consumable":"equipment",names(consumable?"consumable":"equipment"));
        }
        sharedLabel(consumable?"Consumable slot / new value":"Equipment slot / new value");
        return;
    }
    if(alternate && bank==9){
        const auto side=value.kind=="literal"&&value.value==0?QString("army"):
            value.kind=="literal"&&value.value==1?QString("opposing roster"):"side "+number(value);
        action.title="Permanently remove character slot "+number(index)+" from "+side;
        selectIndex("Character slot");selectValue("Character side","named-state",{{0,"Army"},{1,"Opposing roster"}});
        sharedLabel("Character slot / side");
        return;
    }
    if(!alternate && bank==1){
        // The actual squad comes from R7; this helper does not receive it.
        // Keep the caller's existing title rather than inventing its identity.
        selectIndex("Formation-list member position","named-state",{{0,"First member"},{1,"Second member"},{2,"Third member"},{3,"Fourth member"},{4,"Fifth member"}});
        selectValue("Character slot",{}, {},true);
        sharedLabel("Member position / character slot");
    }
}

// Immediate native property writes use the same source field as their literal
// producer. This is presentation only: byte-wide consumers mask SOURCE edits
// to its low byte; packed halfword consumers keep their complete native token.
void describePropertyWrite(StoryAction &action,StoryField &field,int property,int group,int row) {
    const auto definition=catalog()["properties"].toObject()[QString::number(property)].toObject();
    if(property==0xe7 || property==0xe9)return; // Existing scenario controls.
    if(definition["storage"].toObject()["bytes"].toInt()==1){
        field.mask=255;field.shift=0;field.value&=255;
        field.minimum=0;field.maximum=qMin(255,field.maximum);
    }
    const int value=field.value;
    const auto choices=[&](const QString &label,const QString &kind,const QMap<int,QString> &source){
        field.label=label;field.kind=kind;field.choices.clear();
        for(auto it=source.cbegin();it!=source.cend();++it)
            if(it.key()>=field.minimum && it.key()<=field.maximum)field.choices[it.key()]=it.value();
        if(!field.choices.isEmpty() && !field.choices.contains(value))
            field.choices[value]=QString("Existing value %1").arg(value);
    };
    const auto named=[&](const QString &kind,const QString &fallback){
        return names(kind).value(value,QString("%1 %2").arg(fallback).arg(value));
    };
    if(definition["writeBehavior"].toString().contains("infinite loop")){
        field.label=definition["name"].toString()+" input";
        action.title="Unsupported property write · the game stalls";
        return;
    }
    switch(property){
    case 0xe4: {
        choices("Scripted death subject","scripted-death-subject",names("scripted-death-subject"));
        action.title=value==0?"Disable the scripted key-death subject check":value==255?
            "Select the current squad leader for scripted key-death interception":
            "Select scripted key-death identity · "+named("scripted-death-subject","identity token");
        break;
    }
    case 0xe5:
        choices("Current scene skipped marker","named-state",{{0,"Not skipped"},{1,"Skipped"}});
        action.title="Set the current scene skipped marker · "+QString::number(value);
        break;
    case 0xe6: {
        QMap<int,QString> templates{{0,"No scripted opposing-squad override"}};
        const auto catalogTemplates=names("recruitment-template");
        for(auto it=catalogTemplates.cbegin();it!=catalogTemplates.cend();++it){
            if(it.key()<0 || it.key()>32767)continue;
            if(it.key()!=0)templates[it.key()]=it.value()+" · second scene unit: normal";
            templates[it.key()|32768]=it.value()+" · second scene unit: leader only";
        }
        choices("Scripted opposing squad / second scene unit","named-state",templates);
        const auto name=catalogTemplates.value(value&32767,QString("template %1").arg(value&32767));
        action.title=value==0?"Clear the scripted opposing-squad override":
            "Override the scripted opposing squad · "+name+(value&32768?" · second unit leader only":"");
        break;
    }
    case 0xe8:
        choices("Current event row","event-row",names("event-row"));
        action.title="Store current event row · "+EventScript::rowName(value);
        action.detail="The current instruction sequence continues; this stores the row used by the event state.";
        break;
    case 0xeb:
        choices("Birthday history record","scene-history",names("scene-history"));
        action.title=QString("Select birthday history record · %1 [%2]").arg(named("scene-history","history record")).arg(value);
        break;
    case 0xec:
        // These catalog keys are the FULL display token: equipment IDs, or
        // 0x200|consumable ID. Do not reinterpret 531 as equipment ID531 or mask
        // away the consumable selector when changing to another reward.
        choices("Birthday reward-panel item","reward-display",names("reward-display"));
        action.title="Select birthday reward-panel item · "+named("reward-display","reward token");
        break;
    case 0xed:
        field.label="Last processed birthday-gift period";
        action.title=QString("Set last processed birthday-gift period · %1%2").arg(value).arg(value>=99?" · further attempts suppressed":"");
        break;
    case 0xee: field.label="Last birthday celebration year";action.title="Set last birthday celebration year · "+QString::number(value);break;
    case 0xef: field.label="Last birthday celebration month";action.title="Set last birthday celebration month · "+QString::number(value);break;
    case 0xf0: field.label="Last birthday celebration day";action.title="Set last birthday celebration day · "+QString::number(value);break;
    case 0xf9:
        field.label="World location / re-entry value";
        action.title="Set world location / re-entry state · "+QString::number(value);
        break;
    case 0xfa: {
        const bool bypass=qint16(value)>0;
        QMap<int,QString> modes{{0,"Apply cached-location exclusions"},{1,"Bypass cached-location exclusions"},
                                {65535,"Apply cached-location exclusions (stored −1)"}};
        if(!modes.contains(value))modes[value]=QString("%1 (stored %2)").arg(bypass?"Bypass exclusions":"Apply exclusions").arg(qint16(value));
        choices("Successor-mission location checks","named-state",modes);
        action.title=bypass?"Bypass cached-location exclusions for successor missions":"Apply cached-location exclusions for successor missions";
        break;
    }
    case 0xfb:
        field.label="Battle encounter context flags";
        action.title="Set battle encounter context flags · "+QString::number(value);
        break;
    case 0xfd: {
        const auto keys=catalog()["rows"].toObject()[QString::number(row)].toObject()["scenarioKeys"].toArray();
        const auto scope=keys.size()==1?QString("scenario:%1").arg(keys.first().toInt()):QString{};
        const bool squad=field.kind=="scenario-squad" || field.kind=="opposing-squad";
        // Native target state is reused by callers and scene construction.
        // In particular, zero before a scene is not universally "no target",
        // and raw source30 is not opposing index0. Apply no +/-30 conversion.
        if(squad && !scope.isEmpty()){
            const auto squads=names("scenario-squad",scope);
            choices("Scene / opposing squad target","scenario-squad",squads);
            action.title="Select scene dispatch squad · "+squads.value(value,QString("squad/source index %1").arg(value));
        }else if(field.kind=="scenario-site" && !scope.isEmpty()){
            QMap<int,QString> towns;
            const auto sites=names("scenario-site",scope);
            for(auto it=sites.cbegin();it!=sites.cend();++it)if(it.key()>0)towns[it.key()-1]=it.value();
            choices("Contacted town index (zero-based)","named-state",towns);
            action.title="Select contacted-town target · "+towns.value(value,QString("town index %1").arg(value));
        }else{
            field.label=group==3?"Encounter / scene target index":group==4?"Defeat / scene target index":
                group==1 || group==2?"Contact / scene target index (zero-based)":"Scene dispatch target index";
            field.kind.clear();field.choices.clear();
            action.title="Set "+field.label.toLower()+" · "+QString::number(value);
        }
        break;
    }
    case 0xfe: field.label="Source squad index";action.title="Select source squad index · "+QString::number(value);break;
    case 0xff: field.label="Chaos Frame";action.title="Set Chaos Frame · "+QString::number(value);break;
    default:
        field.label=definition["name"].toString("Property input");
        break;
    }
}
