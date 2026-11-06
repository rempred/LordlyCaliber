// Included inside event_flow.cpp's anonymous namespace. These descriptions use
// the verified getter/property contracts, never a retail instruction offset.
bool worldDirectLiteral(const Expression &e) {
    return e.kind=="literal" && e.direct && e.fields.size()==1 &&
           e.fields.first().key=="value" && e.fields.first().value==e.value;
}

void worldLabelLiteral(StoryCondition &c,const Expression &e,const QString &label,
                       const QString &kind={},QMap<int,QString> choices={}) {
    if(!worldDirectLiteral(e))return;
    for(auto &f:c.fields)if(f.offset==e.fields.first().offset && f.key==e.fields.first().key){
        f.label=label;f.kind=kind;f.choices.clear();
        for(auto it=choices.begin();it!=choices.end();++it)
            if(it.key()>=f.minimum && it.key()<=f.maximum)f.choices[it.key()]=it.value();
        if(!f.choices.isEmpty() && !f.choices.contains(f.value))
            f.choices[f.value]=QString("Unlisted value %1").arg(f.value);
    }
}

QString worldPropertyName(int property) {
    switch(property){
    case 0xe5:return "Current scene skipped";
    case 0xe7:return "Staged scenario";
    case 0xe9:return "Selected scenario";
    case 0xeb:return "Birthday history record";
    case 0xec:return "Birthday reward display token";
    case 0xed:return "Last processed birthday-gift period";
    case 0xee:return "Last birthday celebration year";
    case 0xef:return "Last birthday celebration month";
    case 0xf0:return "Last birthday celebration day";
    case 0xf1:return "Birth reference year";
    case 0xf2:return "Chosen birth month";
    case 0xf3:return "Chosen birth day";
    case 0xf4:return "Current calendar year";
    case 0xf5:return "Current calendar month";
    case 0xf6:return "Current calendar day";
    case 0xf9:return "World location and re-entry value";
    default:return {};
    }
}

// Locate an AND without assuming which register held the mask. Keep the mask
// editable: an edited mask must be reinterpreted from its current expression.
bool worldMaskedProperty(const Expression &e,int property,
                         const Expression *&mask) {
    if(e.kind!="calculation" || e.value!=0 || e.children.size()!=2)return false;
    for(int side=0;side<2;++side)
        if(e.children[side].kind=="property" && e.children[side].property==property &&
           e.children[1-side].kind=="literal"){
            mask=&e.children[1-side];return true;
        }
    return false;
}

QString worldLocationName(int token,bool raw) {
    const int id=raw?(token&127):token;
    auto text=names("world-location").value(id,QString("World location ID %1").arg(id));
    if(raw)text+=token&128?" · re-entry requested":" · no re-entry request";
    return text;
}

bool describeWorldCondition(StoryCondition &c,const Expression &left,
                            const Expression &right,int op) {
    const Expression *subject=&left,*literal=&right;
    int comparison=op;
    if(left.kind=="literal" && right.kind!="literal"){
        subject=&right;literal=&left;
        switch(op>>4){
        case 12:comparison=0xd0|(op&15);break;
        case 13:comparison=0xc0|(op&15);break;
        case 14:comparison=0xf0|(op&15);break;
        case 15:comparison=0xe0|(op&15);break;
        }
    }
    const bool equality=comparison>>4==10 || comparison>>4==11;
    const bool notEqual=comparison>>4==11;
    const bool numeric=literal->kind=="literal";
    const bool reversed=subject==&right;
    const auto number=[&]{return QString::number(comparison>>4>=12?qint16(literal->value):literal->value);};
    // Comparison controls store the native operation. Display numeric relations
    // in that same operand order, even when recognition used the reversed side.
    const auto compared=[&](const QString &name,const QString &value){
        return reversed?value+" "+relation(op)+" "+name:name+" "+relation(op)+" "+value;
    };
    const Expression *mask=nullptr;

    if(worldMaskedProperty(*subject,0xf9,mask) && mask->value==127 && numeric){
        worldLabelLiteral(c,*mask,"World location mask");
        worldLabelLiteral(c,*literal,"World location","world-location",names("world-location"));
        c.title=compared("World location",literal->value<=127?worldLocationName(literal->value,false):number());
        return true;
    }
    if(worldMaskedProperty(*subject,0xf9,mask) && mask->value==128 && numeric){
        worldLabelLiteral(c,*mask,"World re-entry mask");
        worldLabelLiteral(c,*literal,"Compared re-entry bits");
        if(equality && literal->value==0)
            c.title=notEqual?"World re-entry is requested":"World re-entry is not requested";
        else c.title=compared("World re-entry bits",number());
        return true;
    }
    if(worldMaskedProperty(*subject,0xfb,mask) && numeric){
        const QMap<int,QString> contexts{{8,"Training battle"},{64,"Random neutral encounter"},
            {128,"Unclaimed elemental Pedra challenge reward eligible"}};
        worldLabelLiteral(c,*mask,"Battle context mask");
        worldLabelLiteral(c,*literal,"Compared battle context bits");
        const auto name=contexts.value(mask->value);
        if(!name.isEmpty() && equality && literal->value==0)
            c.title=name+(notEqual?" is ON":" is OFF");
        else c.title=compared(name.isEmpty()?QString("Battle context bits (mask %1)").arg(mask->value):name+" bits",number());
        return true;
    }
    if(subject->kind=="service" && subject->bank==23 && subject->children.size()==1 && numeric){
        const auto &index=subject->children.first();
        worldLabelLiteral(c,index,"Road","world-road",names("world-road"));
        worldLabelLiteral(c,*literal,"Compared road-unlock value");
        const auto road=index.kind=="literal"?reference("world-road",index.value):index.text;
        c.title="Road · "+road;
        if(equality && literal->value==0)c.title+=notEqual?" is unlocked":" is locked";
        else c.title=compared(c.title+" unlock value",number());
        return true;
    }
    if(subject->kind=="temporary-flag" && subject->children.size()==1 && numeric){
        const auto &index=subject->children.first();
        worldLabelLiteral(c,index,"Temporary gameplay flag ID");
        worldLabelLiteral(c,*literal,"Compared temporary flag value");
        // Only bit17 has a reusable mode name; other IDs keep their identity.
        const auto flag=index.kind=="literal" && index.value==17?QString("Area Investigation mode"):
                        QString("Temporary gameplay flag %1").arg(index.text);
        c.title=flag;
        if(equality && literal->value==0)c.title+=notEqual?" is ON":" is OFF";
        else c.title=compared(c.title+" value",number());
        return true;
    }
    if(subject->kind=="alternate" && subject->bank==11 && subject->children.size()==1 && numeric){
        const auto &index=subject->children.first();
        const QMap<int,QString> elements{{0,"No element"},{1,"Wind"},{2,"Flame"},
                                         {3,"Earth"},{4,"Water"},{5,"Virtue"},{6,"Bane"}};
        worldLabelLiteral(c,index,"Army character");
        worldLabelLiteral(c,*literal,"Element","element",elements);
        c.title=compared("Element of army character "+index.text,elements.value(literal->value,number()));
        return true;
    }
    if(subject->kind=="alternate" && subject->bank==12 && subject->children.size()==1 && numeric){
        const auto &index=subject->children.first();
        worldLabelLiteral(c,index,"Build-stamp query mask");
        worldLabelLiteral(c,*literal,"Compared build-stamp result");
        c.title="Build-stamp query (mask "+index.text+")";
        if(equality && literal->value==0)c.title+=notEqual?" is nonzero":" is zero";
        else c.title=compared(c.title,number());
        return true;
    }
    if(subject->kind!="property")return false;
    const auto name=worldPropertyName(subject->property);
    if(name.isEmpty())return false;
    if(!numeric){
        const auto other=literal->kind=="property"?worldPropertyName(literal->property):QString{};
        c.title=name+" "+relation(comparison)+" "+(other.isEmpty()?literal->text:other);
        return true;
    }
    QMap<int,QString> choices;QString kind;
    if(subject->property==0xe7 || subject->property==0xe9){choices=names("scenario");kind="scenario";}
    if(subject->property==0xe5){choices={{0,"Not skipped"},{1,"Skipped"}};kind="named-state";}
    // Raw F9 comparisons include bit7. Offer the complete encoded byte rather
    // than silently replacing a location's re-entry requirement with a picker.
    if(subject->property==0xf9 && equality){
        for(int value=0;value<256;++value)choices[value]=worldLocationName(value,true);
        kind="named-state";
    }
    const QString fieldLabel=subject->property==0xed?"Birthday-gift period (99+ suppresses attempts)":name;
    worldLabelLiteral(c,*literal,fieldLabel,kind,choices);
    c.title=compared(name,choices.value(literal->value,number()));
    if(!reversed && subject->property==0xed && comparison>>4==14 && literal->value==99)
        c.title="Birthday gift attempts are suppressed (last processed period ≥ 99)";
    return true;
}
