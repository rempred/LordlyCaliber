// Included inside event_flow.cpp's anonymous namespace, before comparisonCondition.
// Retail contracts: parent docs/save-squad-layout-20261005.md establishes five
// saved member positions, empty ID 0, individual IDs below 100, Soldier IDs
// 100..219, and unknown special IDs >=220. docs/character-struct.md establishes
// the 100 retail character records (including fallback record 0). Native member
// getter: z64 ROM 0x46090 / RAM 0x80170190; reviewed vm-semantics.json bank 1.
// The iterator proof establishes recurring uses, not complete visitation. Never
// turn a boundary comparison into a claim that every earlier slot was checked.
bool describeRosterScan(StoryCondition &condition,const Expression &left,const Expression &right,int op){
    if(left.text.isEmpty()||right.text.isEmpty())return false;
    const bool reversed=left.kind=="literal";
    const auto &subject=reversed?right:left,&number=reversed?left:right;
    if(number.kind!="literal")return false;
    const bool memberIndex=subject.kind=="member-index";
    const bool armyIndex=subject.kind=="army-index",opposingIndex=subject.kind=="opposing-index";
    const bool member=subject.kind=="member"&&subject.byte;
    if(!memberIndex&&!armyIndex&&!opposingIndex&&!member)return false;
    int test=op>>4;
    if(test<10||test>15)return false;
    if(reversed){if(test==12)test=13;else if(test==13)test=12;else if(test==14)test=15;else if(test==15)test=14;}
    // Ordering is signed in the native VM; equality compares the full bits.
    const int cutoff=(op>>4)>=12?qint16(number.value):number.value;
    const auto labelCutoff=[&](const QString &label){
        for(auto &field:condition.fields)if(fromLiteral(field,number))field.label=label;
    };
    const auto comparison=[&](const QString &name){
        const QStringList words{"equals","does not equal","is greater than","is less than","is at least","is at most"};
        return QString("%1 %2 %3").arg(name,words[test-10]).arg(cutoff);
    };
    if(memberIndex||armyIndex||opposingIndex){
        const bool passed=memberIndex?((test==14&&cutoff==5)||(test==12&&cutoff==4)):
            ((test==14&&cutoff==100)||(test==12&&cutoff==99));
        const bool before=memberIndex?((test==13&&cutoff==5)||(test==15&&cutoff==4)):
            ((test==13&&cutoff==100)||(test==15&&cutoff==99));
        const QString scan=memberIndex?"Squad member scan":armyIndex?"Army character scan":"Opposing character scan";
        const QString limit=memberIndex?"five-position limit":"100-slot limit";
        if(passed)condition.title=scan+" reached its "+limit;
        else if(before)condition.title=scan+" has not reached its "+limit;
        else if(memberIndex&&test==10&&cutoff>=0&&cutoff<5){
            const QStringList positions{"first","second","third","fourth","fifth"};
            condition.title=scan+" is at the "+positions[cutoff]+" squad position";
        }
        else condition.title=comparison(memberIndex?"Squad member scan position":armyIndex?"Army character scan slot":"Opposing character scan slot");
        labelCutoff(memberIndex?"Member scan cutoff (zero-based)":"Character scan cutoff (zero-based)");
        return true;
    }

    // A member ID >99 includes unknown values 220..255. Do not call that test
    // proof of a valid Soldier group, or call IDs below 100 occupied characters:
    // zero is an empty member. Exact known Soldier IDs can name their pool slot.
    if((test==14&&cutoff==100)||(test==12&&cutoff==99)){
        condition.title=subject.text+" is outside the individual-character range";
    }else if((test==13&&cutoff==100)||(test==15&&cutoff==99)){
        condition.title=subject.text+" is in the individual-character range (may be empty)";
    }else if((test==10||test==11)&&cutoff>=100&&cutoff<220){
        condition.title=subject.text+(test==10?" references Soldier group slot ":" does not reference Soldier group slot ")+QString::number(cutoff-100+1);
    }else{
        // Preserve the existing present/absent descriptions for empty-ID tests.
        if(cutoff==0&&(test==10||test==11))return false;
        condition.title=comparison(subject.text+" member ID");
    }
    labelCutoff("Member slot cutoff");
    return true;
}
