// A closed native value-selection diamond reads the owned item count, or a
// literal fallback when the inventory lookup returns 511. It changes the event
// comparison value, not the owned inventory count. Include inside event_flow's
// anonymous namespace, after the expression/comparison helpers.
struct QuantitySetupGroup {
    int count=0;
    StoryAction action;
};
namespace quantity_setup {
bool closed(const QByteArray &bytes,const QSet<int> &offsets,const QMap<int,QSet<int>> &incoming,int at,int count,int end){
    const int stop=at+count*2;
    if(at<0||(at&1)||stop>end||stop>bytes.size())return false;
    for(int pc=at;pc<stop;pc+=2){
        if(!offsets.contains(pc))return false;
        if(pc!=at)for(int source:incoming.value(pc))if(source<at||source>=stop)return false;
    }
    return true;
}
StoryField itemField(int at,int value,bool wide,bool equipment){
    const QString kind=equipment?"equipment":"consumable";
    StoryField f{equipment?"Equipment":"Consumable","value",at,value,0,wide?65535:255};
    f.kind=kind;f.choices=names(kind);
    for(auto i=f.choices.begin();i!=f.choices.end();)if(i.key()<f.minimum||i.key()>f.maximum)i=f.choices.erase(i);else ++i;
    if(!f.choices.contains(value))f.choices[value]=QString("%1 ID %2").arg(equipment?"Equipment":"Consumable").arg(value);
    return f;
}
}
QuantitySetupGroup groupQuantitySetup(const QByteArray &bytes,const QSet<int> &offsets,
                                     const QMap<int,QSet<int>> &incoming,int at,int end){
    QuantitySetupGroup group;
    if(at<0||at+1>=bytes.size()||u8(bytes,at)!=0x6f)return group;
    const bool wide=at+3<bytes.size()&&u8(bytes,at+2)==0x8f;
    const int lookup=at+(wide?4:2),count=wide?9:8;
    if(!quantity_setup::closed(bytes,offsets,incoming,at,count,end))return group;
    const bool equipment=u16(bytes,lookup)==0x782f;
    if(!equipment&&u16(bytes,lookup)!=0x780f)return group;
    if(u16(bytes,lookup+2)!=0x69ff||u16(bytes,lookup+4)!=0x8901||
       u16(bytes,lookup+6)!=0xa102||u16(bytes,lookup+8)!=(equipment?0x7a30:0x7a10)||
       u16(bytes,lookup+10)!=0x0101||u8(bytes,lookup+12)!=0x6a)return group;
    const int value=u8(bytes,at+1)|(wide?u8(bytes,at+3)<<8:0),fallbackAt=lookup+12,defaultCount=u8(bytes,fallbackAt+1);
    const QString item=reference(equipment?"equipment":"consumable",value);
    const QString quantity=QString("Owned copies of %1 (%2 if no inventory entry)").arg(item).arg(defaultCount);
    const auto itemField=quantity_setup::itemField(at,value,wide,equipment);
    const StoryField fallback{"Count when no inventory entry","value",fallbackAt,defaultCount,0,255};
    group.count=count;group.action.kind="action";group.action.title="Read "+quantity;
    group.action.fields={itemField,fallback};
    group.action.detail="Use this count for the following event logic.";
    const int thresholdAt=fallbackAt+2,test=thresholdAt+2;
    // The common consumer compares R2 (selected count) with R3 (literal
    // threshold). Retain the current operator, including an edited relation.
    if(!quantity_setup::closed(bytes,offsets,incoming,at,count+2,end)||
       u8(bytes,thresholdAt)!=0x6b||!conditional(u8(bytes,test))||(u8(bytes,test)&15)!=11)return group;
    const int op=u8(bytes,test),threshold=u8(bytes,thresholdAt+1);
    const int target=test+2+qint8(u8(bytes,test+1))*2;
    if(target>at&&target<test+2)return group;
    auto &action=group.action;group.count=count+2;
    action.kind="condition";action.fields.clear();action.detail.clear();
    action.title=quantity+" "+relation(op)+" "+QString::number(threshold);
    action.condition={"test",action.title};
    StoryField compared{"Owned count threshold","value",thresholdAt,threshold,0,255};
    StoryField comparison{"Comparison","comparison",test,op>>4,10,15};comparison.kind="named-state";
    comparison.choices={{10,"Equals"},{11,"Does not equal"},{12,"Greater than"},{13,"Less than"},{14,"At least"},{15,"At most"}};
    action.condition.fields={itemField,fallback,compared,comparison};
    action.target=target;action.otherwise=test+2;
    return group;
}

// Mixed setup cards cannot use the all-literal scan callback directly. Keep
// every existing word/card in place and describe the additional proved values.
// describeScanStart() must be defined before including this file.
bool describeQuantityScalarSetup(StoryAction &action,const QByteArray &bytes,const QSet<int> &offsets,
                                 const QMap<int,QSet<int>> &incoming,const QMap<int,Registers> &iterators,int group){
    if(!action.advanced||action.instructions.isEmpty())return false;
    const int at=action.instructions.first(),count=action.instructions.size();
    if(!quantity_setup::closed(bytes,offsets,incoming,at,count,bytes.size()))return false;
    for(int n=0;n<count;++n)if(action.instructions[n]!=at+n*2)return false;
    const int source=u8(bytes,at+1);
    if(count==3&&u8(bytes,at)==0x67&&
       ((source==0xfd&&(group==3||group==4))||(source==0xfe&&group>=1&&group<=4))){
        auto scan=action;scan.instructions=scan.instructions.mid(1);
        if(!describeScanStart(scan,bytes,iterators))return false;
        // R7 selects the member service's squad. The tail's must-analysis
        // separately proves the member-position initializer and its cutoff.
        action.title=scan.title+" · "+propertyLabel(source,group);
        action.detail="Use the selected squad's member list.";
        action.fields=scan.fields;action.kind="action";action.advanced=false;return true;
    }
    if(count!=4||at<4||at+33>=bytes.size()||u8(bytes,at)!=0x6e||u8(bytes,at+2)!=0x6f||
       u8(bytes,at+4)!=0x6a||u8(bytes,at+6)!=0x69)return false;
    // Complete first-match identity lookup: save caller R1/R2, initialize the
    // returned R6/R7 pair, scan army records, copy the first match, restore and
    // return. The four initializer operands and equivalent predicate location
    // may vary; all control flow, indexing and return-value transfers must match.
    const QMap<int,int> fixed{{-4,0x0501},{-2,0x0502},{8,0xd609},{10,0x641a},
        {16,0xac03},{18,0x6604},{20,0x6722},{22,0x0102},{24,0x9a41},
        {26,0x01f6},{28,0x0602},{30,0x0601},{32,0x0400}};
    for(int pc=at-4;pc<=at+32;pc+=2)if(!offsets.contains(pc))return false;
    for(auto i=fixed.cbegin();i!=fixed.cend();++i)if(u16(bytes,at+i.key())!=i.value())return false;
    if(u8(bytes,at+12)!=0x68||u8(bytes,at+14)!=3)return false;
    const int call=at+14;
    const Expression displacement{QString::number(u8(bytes,at+13)),"literal",u8(bytes,at+13)};
    const auto helper=EventHelperNames::match(bytes,offsets,localTarget(call,u8(bytes,call+1),displacement));
    if(helper.kind!=EventHelperNames::Kind::IdentityFamily)return false;
    auto scan=action;scan.instructions=scan.instructions.mid(2);
    if(!describeScanStart(scan,bytes,iterators))return false;
    const auto returned=[&](int pc,const QString &label){
        StoryField f{label,"value",pc,u8(bytes,pc+1),0,255};f.kind="identity-class";f.choices=names("identity-class");
        for(auto i=f.choices.begin();i!=f.choices.end();)if(i.key()<0||i.key()>255)i=f.choices.erase(i);else ++i;
        if(!f.choices.contains(f.value))f.choices[f.value]=QString("Class ID %1").arg(f.value);
        return f;
    };
    const auto identity=returned(at,"Identity returned when no match"),current=returned(at+2,"Current class returned when no match");
    action.title=QString("Start army identity lookup at slot %1; no match returns %2 / %3")
        .arg(u8(bytes,at+5)).arg(identity.choices[identity.value],current.choices[current.value]);
    action.detail="Return the primary identity and current class of the first matching army character.";
    action.fields={identity,current};appendFields(action.fields,scan.fields);
    action.kind="action";action.advanced=false;return true;
}
