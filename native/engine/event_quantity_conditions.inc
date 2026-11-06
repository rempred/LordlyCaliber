// Included after Expression and its helpers, inside event_flow.cpp's anonymous
// namespace. These descriptions change presentation only. Native contracts:
// parent docs/event-semantics/vm-reference.md, arithmetic and alternate services.
namespace quantity_conditions {
enum Domain { Unknown, Time, ClockPart, Random, InventorySlot, Count, Calculation };
struct Description {
    QString text, threshold;
    Domain domain=Unknown;
    bool countArithmetic=false,clockArithmetic=false,randomArithmetic=false;
};
bool sameField(const StoryField &a,const StoryField &b){
    return a.offset==b.offset&&a.key==b.key&&a.mode==b.mode;
}
void labelSource(StoryCondition &c,const Expression &source,const QString &label){
    // A folded literal may have several producers or a low-byte truncation.
    // Calling those individual inputs the final threshold would be misleading.
    if(source.kind!="literal"||source.fields.size()!=1)return;
    const auto &input=source.fields.first();
    if(input.value!=source.value||(!source.direct&&input.key!="argument"))return;
    for(auto &f:c.fields)if(sameField(f,input))f.label=label;
}
void hideFixed(StoryCondition &c,const Expression &source,const Expression &other){
    for(const auto &input:source.fields){
        // A producer shared with the compared operand still needs its control.
        if(std::any_of(other.fields.begin(),other.fields.end(),[&](const auto &f){return sameField(f,input);}))continue;
        c.fields.erase(std::remove_if(c.fields.begin(),c.fields.end(),[&](const auto &f){return sameField(f,input);}),c.fields.end());
    }
}
void labelThreshold(StoryCondition &c,const Expression &source,const Expression &subject,const QString &label){
    auto previous=c.fields;labelSource(c,source,label);
    for(int n=0;n<c.fields.size();++n)if(c.fields[n].label!=previous[n].label&&
        std::any_of(subject.fields.begin(),subject.fields.end(),[&](const auto &f){return sameField(f,c.fields[n]);}))
        c.fields[n].label=previous[n].label+" / "+label;
}
bool clock(const Expression &e,const QString &part){return e.kind=="clock"&&e.text=="Clock "+part;}
const Expression *hourScale(const Expression &e){
    if(e.kind!="calculation"||e.value!=4||e.children.size()!=2)return nullptr;
    for(int n=0;n<2;++n)if(clock(e.children[n],"hour")&&e.children[1-n].kind=="literal"&&e.children[1-n].value==100)
        return &e.children[1-n];
    return nullptr;
}
QString itemName(const Expression &slot){
    if(slot.children.isEmpty())return slot.text;
    const auto &item=slot.children.first();
    return item.kind=="literal"?reference(slot.kind=="consumable-slot"?"consumable":"equipment",item.value):item.text;
}
QString number(const Expression &e,int op){return QString::number((op>>4)>=12?qint16(e.value):e.value);}
Description describe(const Expression &e,StoryCondition &c,const Expression &other,int op){
    if(e.kind=="calculation"&&e.value==2&&e.children.size()==2){
        for(int n=0;n<2;++n)if(clock(e.children[n],"minute"))if(const auto *scale=hourScale(e.children[1-n])){
            hideFixed(c,*scale,other);return {"Clock time","Time (HHMM)",Time,false,true};
        }
    }
    if(e.kind=="clock"){
        const QString part=e.text.mid(QString("Clock ").size());
        return {e.text,part.left(1).toUpper()+part.mid(1)+" threshold",ClockPart,false,true};
    }
    if(e.kind=="property"&&e.property==0xea)return {"Pseudorandom value (0–32767)","Random threshold",Random,false,false,true};
    if(e.kind=="consumable-slot"||e.kind=="equipment-slot")return {e.text,"Inventory slot result",InventorySlot};
    if(e.kind=="alternate"&&QSet<int>{2,4,6,8}.contains(e.bank)&&e.children.size()==1){
        const bool consumable=e.bank<5,owned=e.bank==2||e.bank==6;
        const auto &index=e.children.first();
        const QString quantity=owned?"Owned copies":consumable?"Carried copies":"Equipped copies";
        if(index.kind==(consumable?"consumable-slot":"equipment-slot"))
            return {quantity+" of "+itemName(index),quantity,Count};
        labelSource(c,index,consumable?"Consumable inventory slot":"Equipment inventory slot");
        return {QString("%1 %2 in inventory slot %3").arg(consumable?"Consumable":"Equipment",quantity.toLower(),index.text),quantity,Count};
    }
    if(e.kind=="calculation"&&e.children.size()==2&&e.value>=0&&e.value<7){
        const auto &a=e.children[0],&b=e.children[1];
        if(e.value==6&&a.kind=="property"&&a.property==0xea){
            labelSource(c,b,"Random range size");
            if(b.kind=="literal"&&b.value>0)
                return {QString("Pseudorandom roll (0–%1)").arg(qMin(32767,b.value-1)),"Random threshold",Random,false,false,true};
            // A zero divisor reaches the native break. Never label it a valid
            // random range, and retain the editable divisor so it can be fixed.
            return {QString("Pseudorandom remainder (divisor %1%2)").arg(b.text,b.kind=="literal"&&b.value==0?"; division by zero":""),"Random threshold",Random,false,false,true};
        }
        const auto left=describe(a,c,other,op),right=describe(b,c,other,op);
        if(left.domain!=Unknown||right.domain!=Unknown){
            const QStringList signs{"&","|","+","−","×","÷","remainder"};
            const bool count=left.domain==Count||right.domain==Count||left.countArithmetic||right.countArithmetic;
            const bool time=left.clockArithmetic||right.clockArithmetic,random=left.randomArithmetic||right.randomArithmetic;
            const QString quantity=count?"Count":time?"Clock":random?"Random":"Result";
            const QStringList roles{quantity+" bit mask",quantity+" bit flags",quantity+" increment",quantity+" decrement",quantity+" multiplier",quantity+" divisor",quantity+" remainder divisor"};
            if(left.domain!=Unknown)labelSource(c,b,roles[e.value]);
            if(right.domain!=Unknown)labelSource(c,a,e.value==3?quantity+" subtraction starting value":roles[e.value]);
            return {QString("(%1 %2 %3)").arg(left.domain==Unknown?a.text:left.text,signs[e.value],right.domain==Unknown?b.text:right.text),quantity+" result threshold",Calculation,count,time,random};
        }
    }
    return {e.kind=="literal"?number(e,op):e.text,{},Unknown};
}
QString thresholdText(const Expression &e,const Description &subject,int op){
    if(e.kind!="literal")return e.text;
    if(subject.domain==Time){
        const int value=(op>>4)>=12?qint16(e.value):e.value;
        if(value>=0&&value<=2359&&value%100<60)
            return QString("%1:%2").arg(value/100,2,10,QChar('0')).arg(value%100,2,10,QChar('0'));
        return QString("%1 (HHMM value)").arg(value);
    }
    return number(e,op);
}
}
bool describeQuantityCondition(StoryCondition &condition,const Expression &left,const Expression &right,int op){
    using namespace quantity_conditions;
    StoryCondition c=condition;
    const auto a=describe(left,c,right,op),b=describe(right,c,left,op);
    if(a.domain==Unknown&&b.domain==Unknown)return false;
    if((op>>4==10||op>>4==11)&&
       ((a.domain==InventorySlot&&right.kind=="literal"&&right.value==511)||
        (b.domain==InventorySlot&&left.kind=="literal"&&left.value==511))){
        const auto &slot=a.domain==InventorySlot?left:right;
        const auto &sentinel=a.domain==InventorySlot?right:left;
        hideFixed(c,sentinel,slot);
        c.title=itemName(slot)+(op>>4==10?" has no inventory entry":" has an inventory entry");
    }else{
        if(a.domain!=Unknown)labelThreshold(c,right,left,a.threshold);
        if(b.domain!=Unknown)labelThreshold(c,left,right,b.threshold);
        const QString lhs=a.domain==Unknown?thresholdText(left,b,op):a.text;
        const QString rhs=b.domain==Unknown?thresholdText(right,a,op):b.text;
        // Keep native operand order: the comparison editor controls that same
        // stored relation. Arithmetic is not simplified across 16-bit wrapping.
        c.title=lhs+" "+relation(op)+" "+rhs;
        if(a.domain==Calculation||b.domain==Calculation)c.title+=" (16-bit result)";
    }
    condition=c;return true;
}
