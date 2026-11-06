// Include after comparisonCondition(). Missing dataflow does not make the
// native conditional branch opaque: its operator and register layout are known.
// Retail roles below are instruction-local descriptions, never propagated facts.
namespace conditional_fallback {
struct RetailStep { int word=-1; QString summary; };
const RetailStep *retailStep(int row,int at,int word){
    static const auto steps=[] {
        QMap<int,QMap<int,RetailStep>> result;
        const auto rows=catalog()["rows"].toObject();
        for(auto it=rows.begin();it!=rows.end();++it)for(const auto &v:it.value().toObject()["steps"].toArray()){
            const auto s=v.toArray();const int word=s[3].toInt();
            if(conditional(word>>8))result[it.key().toInt()][s[0].toInt()]={word,s[1].toString()};
        }
        return result;
    }();
    const auto program=steps.constFind(row);if(program==steps.cend())return nullptr;
    const auto step=program->constFind(at);
    return step!=program->cend()&&step->word==word?&*step:nullptr;
}
QStringList operandDescriptions(const QString &summary){
    // Narrative introductions may precede the actual "If" clause. Assignment
    // expansions may contain another '=' before the final comparison operator.
    const int start=summary.lastIndexOf("If ");if(start<0)return {};
    QString predicate=summary.mid(start+3);
    static const QRegularExpression tail(",\\s*branch\\b|;\\s*(?:false|true)\\s*→|,\\s*take the true branch\\b");
    const auto end=tail.match(predicate);if(end.hasMatch())predicate.truncate(end.capturedStart());
    static const QRegularExpression comparator("\\s*(?:>=|<=|!=|=|≠|>|<|≥|≤)\\s*");
    auto comparisons=comparator.globalMatch(predicate);int begin=-1,length=0;
    while(comparisons.hasNext()){const auto m=comparisons.next();begin=m.capturedStart();length=m.capturedLength();}
    if(begin<0)return {};
    return {predicate.left(begin).trimmed(),predicate.mid(begin+length).trimmed()};
}
QStringList narrativeDescriptions(const QString &summary,int op){
    QString left,right;
    const auto has=[&](const QString &pattern){return QRegularExpression(pattern,QRegularExpression::CaseInsensitiveOption).match(summary).hasMatch();};
    if(summary.startsWith("CHECK contacted site")){left="Contacted town number (one-based)";right="Required town number";}
    else if(summary.startsWith("CHECK Flag ")){
        const auto end=summary.indexOf(" = ");if(end>0)left=summary.mid(6,end-6);
    }else if(has("(?:owned quantity|already\\s*99|already99).*(?:greater|more)|already99 or greater")){
        const bool reversed=(op>>4)==15;left=reversed?"Inventory count limit":"Owned item count";right=reversed?"Owned item count":"Inventory count limit";
    }else if(has("all\\s*(?:40|278).*?(?:slots|positions).*?(?:exhausted|occupied)")){
        const bool reversed=(op>>4)==15;left=reversed?"Inventory slot limit":"Inventory scan position";right=reversed?"Inventory scan position":"Inventory slot limit";
    }else if(has("candidate.*slot.*occupied|candidate is occupied|occupied candidate.*slot|slot already has a nonzero equipment ID"))left="Item ID in candidate inventory slot";
    else if(has("living.*Petrifi"))left="Character viability test result";
    else if(has("Petrifi|petrified"))left="Petrification test result";
    else if(has("(?:HP\\s*(?:is)?\\s*(?:0|zero)|current HP is zero|zero HP)")){
        const bool opposing=summary.contains("opposing",Qt::CaseInsensitive),soldier=summary.contains("Soldier");
        left=soldier?(opposing?"HP of checked opposing Soldier":"HP of checked army Soldier"):
            opposing?"HP of checked opposing character":summary.contains("army character")?"HP of checked army character":"HP of checked character";
        right="HP threshold";
    }
    else if(has("native (?:identity family|.*identity.*test)|identity matches|native identity family|identity.*match|identity (?:predicate|test|comparison) returned|this record is not"))left="Character identity match result";
    else if(has("exclusion predicate|excluded transformed class|(?:Zombie|Skeleton|Ghost|Angel Knight|Seraph).*current class|current class.*(?:Zombie|Skeleton|Ghost|Angel Knight|Seraph)"))left="Excluded current-class test result";
    else if(has("candidate current class.*Lich or Vampire"))left="Lich or Vampire current-class match result";
    else if(has("current class equals")){left="current class";right="Required current class";}
    else if(has("this birthday guest is not")){left="Birthday guest identity";right="Required birthday guest identity";}
    else if(has("current birthday-period ordinal exceeds saved ordinal")){left="Current birthday period number";right="Previous gift birthday period number";}
    else if(has("(?:current|input date) month is later than birthday month")){left="Chosen birth month";right=summary.contains("input date")?"Input date month":"Current calendar month";}
    else if(has("(?:Army-loader flag test|flags&0x81|source squad.*flag)")){left="Squad flags test result";right="Required squad flag result";}
    else if(has("(?:source squad index|target squad index).*(?:30|thirty)")){
        const bool reversed=(op>>4)==13||(op>>4)==15;left=reversed?"Roster boundary":"Squad index";right=reversed?"Squad index":"Roster boundary";
    }else if(has("(?:member|Soldier).*IDs? above\\s*99|member ID above99")){
        const bool reversed=(op>>4)==13||(op>>4)==15;left=reversed?"Last ordinary character slot":"Squad member ID";right=reversed?"Squad member ID":"Last ordinary character slot";
    }else if(has("all\\s*278 slots have been searched")){left="Equipment inventory scan position";right="Equipment inventory slot limit";}
    else if(has("After (?:record\\s*99|all five positions)|five positions (?:without|lack)|record99")){
        const bool reversed=(op>>4)==13||(op>>4)==15;left=reversed?"Scan limit":"Character or formation scan position";right=reversed?"Character or formation scan position":"Scan limit";
    }
    else if(has("transient index\\s*127|new-campaign reset loop")){left="Temporary flag reset index";right="Temporary flag reset limit";}
    else if(has("current age ordinal|last-celebration age ordinal")){left="Birthday age comparison value";right=left;}
    else if(has("annual period|selected birthday day")){left="Birthday date comparison value";right=left;}
    else if(has("training battles|training battle"))left="Training-battle context test result";
    else if(has("empty formation entry|this position is empty"))left="Squad member ID";
    else if(has("(?:Quetzalcoatl|Flarebrass|Ahzi Dahaka|Hydra) matches")){
        const auto subject=QRegularExpression("If (.+?) matches").match(summary);
        left=subject.captured(1)+" identity match result";
    }else if(summary.contains("has not reached39")){left="World-location reset index";right="Last world-location reset index";}
    else if(summary.contains("has not reached40")){left="World-road reset index";right="Last world-road reset index";}
    else if(summary.contains("has reached128")){left="Temporary gameplay flag reset index";right="Temporary gameplay flag reset limit";}
    if(left.isEmpty())return {};
    const bool single=(op&15)>=12;const int a=single?op&3:(op>>2)&3,b=op&3;
    return {QString("R%1 (%2)").arg(a).arg(left),right.isEmpty()?QString{}:QString("R%1 (%2)").arg(b).arg(right)};
}
QString capitalized(QString text){if(!text.isEmpty())text[0]=text.at(0).toUpper();return text;}
Expression role(const QString &description,int reg){
    Expression result;result.kind="dynamic";
    for(const auto &prefix:{QString("raw member record ID "),QString("transient flag index ")})
        if(description.startsWith(prefix))return role(description.mid(prefix.size()),reg);
    const QRegularExpression direct(QString("^R%1\\s*\\(").arg(reg));
    const auto annotation=direct.match(description);
    if(annotation.hasMatch()){
        const int start=annotation.capturedEnd();int end=start,depth=1;
        for(;end<description.size();++end){if(description[end]=='(')++depth;else if(description[end]==')'&&!--depth)break;}
        if(depth)return {};
        QString name=description.mid(start,end-start).trimmed();
        if(name=="comparison value"||name=="calculated value"||name=="caller value"||name=="saved register value")return {};
        if(name=="character identity"||name=="required character identity/class"||name=="Birthday guest identity"||name=="Required birthday guest identity"){
            result.text=name=="character identity"?"Character identity":name=="required character identity/class"?"Required character identity/class":name;
            result.kind="identity-class";
        }else if(name=="current class"||name=="current class to check"||name=="comparison value for current class to check"){
            result.text=name.startsWith("comparison")?"Required current class":"Current class of checked character";result.kind="current-class";
        }else if(name=="match result: yes/no")result.text="Match result";
        else if(name=="saved squad member ID")result.text="Saved squad member ID";
        else if(name=="annual birthday-period ordinal")result.text="Birthday period number";
        else if(name.startsWith("comparison value for five birthday guest slots"))result.text="Birthday guest slot limit";
        else if(name=="consumable quantity")result.text="Consumable quantity";
        else if(name=="equipment quantity")result.text="Equipment quantity";
        else if(name=="equipment inventory item halfword")result.text="Equipment ID in inventory slot";
        else if(name=="consumable inventory item halfword")result.text="Consumable ID in inventory slot";
        else if(name=="Petrification state"||name=="Petrification test")result.text="Petrification test result";
        else if(name=="clock hour")result.text="Clock hour";
        else if(name=="clock minute")result.text="Clock minute";
        else if(name=="clock second")result.text="Clock second";
        else result.text=capitalized(name);
        static const QRegularExpression calculated("\\s(?:[&|+×÷−]|-)\\s|\\bremainder\\b|\\btruncated\\b");
        if(calculated.match(description).hasMatch()&&!result.text.contains("result",Qt::CaseInsensitive)){
            result.text+=" calculation result";result.kind="dynamic";
        }
        return result;
    }
    if(description.startsWith("low byte of ",Qt::CaseInsensitive)){
        const auto source=description.mid(12);
        // This is a proved register-byte copy, so the top-level source's role
        // remains relevant even when the comparison uses a different register.
        const auto copied=QRegularExpression("^R([0-7])\\s*\\(").match(source);
        result=role(source,copied.hasMatch()?copied.captured(1).toInt():reg);
        if(!result.text.isEmpty()){result.text="Low byte of "+result.text;result.kind="dynamic";}
        return result;
    }
    // Expanded source expressions can mention other registers as historical
    // indexes. Do not borrow their annotations as roles for the tested value.
    if(description.startsWith("army character current HP")||description.startsWith("opposing character current HP")||
       description.startsWith("army Soldier current HP")||description.startsWith("opposing Soldier current HP")){
        const bool soldier=description.contains("Soldier");
        result.text=description.startsWith("army")?(soldier?"HP of checked army Soldier":"HP of checked army character"):
            (soldier?"HP of checked opposing Soldier":"HP of checked opposing character");result.kind="hp";
    }else if(description.startsWith("army character primary identity")||description.startsWith("opposing character primary identity")){
        result.text=description.startsWith("army")?"Identity of checked army character":"Identity of checked opposing character";result.kind="identity-class";
    }else if(description.startsWith("army character secondary class")||description.startsWith("opposing character secondary class")){
        result.text=description.startsWith("army")?"Current class of checked army character":"Current class of checked opposing character";result.kind="current-class";
    }else if(description.startsWith("army character status")||description.startsWith("opposing character status")){
        static const QRegularExpression petrification("&\\s*(?:4|0x0?4)\\b");
        result.text=petrification.match(description).hasMatch()?"Petrification test result":
            description.startsWith("army")?"Status of checked army character":"Status of checked opposing character";
    }
    else if(description.startsWith("dispatch source squad index"))result.text="Acting squad index";
    else if(description.startsWith("dispatch target zero-based index"))result.text="Contact target number (zero-based)";
    else if(description.startsWith("dispatch target one-based index"))result.text="Contact target number (one-based)";
    else if(description.startsWith("squad saved flags"))result.text="Saved squad flags";
    else if(description.startsWith("squad member at squad"))result.text="Squad member ID";
    else if(description.startsWith("persistent story flag at "))result.text="Story flag result";
    else if(description.startsWith("Flag ")){
        // The reviewed text already carries the exact instruction's flag name.
        // Keep it as a label, without claiming a recoverable editable flag ID.
        result.text=description;
    }else if(description.startsWith("find equipment inventory slot"))result.text="Equipment inventory slot result";
    else if(description.startsWith("find consumable inventory slot"))result.text="Consumable inventory slot result";
    else if(description.startsWith("equipment owned count"))result.text="Owned equipment count";
    else if(description.startsWith("consumable owned count"))result.text="Owned consumable count";
    else if(description.startsWith("equipment assigned count"))result.text="Equipped equipment count";
    else if(description.startsWith("consumable assigned count"))result.text="Carried consumable count";
    else if(description.contains("game clock hour")||description.contains("game-clock hour"))result.text="Clock calculation result";
    else if(description.startsWith("15-bit pseudorandom value"))result.text="Pseudorandom calculation result";
    else if(description.startsWith("Chaos Frame"))result.text="Chaos Frame";
    else if(description=="Dragon's Scale count"||description=="Letter from Father count"||description=="Package for Gelda count")
        result.text="Owned copies of "+description.left(description.size()-6);
    static const QRegularExpression calculated("\\s(?:[&|+×÷−]|-)\\s|\\bremainder\\b|\\btruncated\\b");
    if(!result.text.isEmpty()&&calculated.match(description).hasMatch()&&!result.text.contains("result",Qt::CaseInsensitive)){
        result.text+=" calculation result";result.kind="dynamic";
    }
    return result;
}
Expression operand(const Expression &known,const QString &description,int reg,bool other){
    if(!known.text.isEmpty())return known;
    auto result=role(description,reg);
    if(result.text.isEmpty())result={other?"Other dynamic event value":"Dynamic event value","dynamic"};
    result.fields=known.fields;
    return result;
}
}
StoryCondition unresolvedCondition(const QByteArray &bytes,int at,const Registers &r,int row,bool trustedRetail){
    if(at<0||(at&1)||at+1>=bytes.size()||!conditional(u8(bytes,at)))return {};
    const int op=u8(bytes,at);const bool single=(op&15)>=12;
    const int a=single?op&3:(op>>2)&3,b=op&3;
    QStringList descriptions;
    if(trustedRetail)if(const auto *step=conditional_fallback::retailStep(row,at,u16(bytes,at))){
        descriptions=conditional_fallback::operandDescriptions(step->summary);
        if(descriptions.isEmpty())descriptions=conditional_fallback::narrativeDescriptions(step->summary,op);
        // The old review recorded the first iteration as "1 < 5". Current
        // native words 7222/7224 use R1 as the guest cast slot, 7226 increments
        // it, and 7232 loops while R1 < 5. It is a varying position, not 1.
        if(row==67&&at==7232)descriptions={"R1 (Birthday guest cast position)","R2 (Birthday guest cast slot limit)"};
    }
    const auto left=conditional_fallback::operand(r[a],descriptions.value(0),a,false);
    const auto right=single?Expression{"0","literal",0}:
        conditional_fallback::operand(r[b],descriptions.value(1),b,left.text=="Dynamic event value");
    auto c=comparisonCondition(left,right,op);
    const bool reverse=left.kind=="literal";const auto &subject=reverse?right:left,&literal=reverse?left:right;
    if(literal.kind=="literal"&&subject.kind=="dynamic")
        for(auto &f:c.fields)if(fromLiteral(f,literal))f.label=subject.text+" threshold";
    return c;
}
