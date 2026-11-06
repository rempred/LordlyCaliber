#include "engine/consumable_tree.h"
#include "engine/consumable_effects.h"
#include <QJsonDocument>
#include <functional>
namespace lordly {
namespace {
QStringList effectIds(){return {"heal","fatigue","cure","revive","level","maxHp","strength","vitality","intelligence","mentality","agility","dexterity","alignment","luck"};}
QStringList conditionIds(){return {"retail","alive","dead","injured","petrified","ordinary","soldier","hpPercent","class","fatigue","alignment","luck","army","map"};}
int integer(const QJsonObject &o,const QString &key,int low,int high,int fallback=0){
    const auto v=o.value(key);if(v.isUndefined())return fallback;
    if(!v.isDouble()||v.toDouble()!=v.toInt()||v.toInt()<low||v.toInt()>high)throw Error("Invalid consumable tree value: "+key);
    return v.toInt();
}
}
bool ConsumableTree::supported(int item){return item>=1&&item<=19&&item!=8;}
QMap<QString,QString> ConsumableTree::actions(){return {{"heal","Restore HP"},{"fatigue","Reduce fatigue"},{"cure","Remove petrification"},{"revive","Revive and remove petrification"},{"level","Level up"},{"maxHp","Change maximum HP"},{"strength","Change strength"},{"vitality","Change vitality"},{"intelligence","Change intelligence"},{"mentality","Change mentality"},{"agility","Change agility"},{"dexterity","Change dexterity"},{"alignment","Change alignment"},{"luck","Change luck"}};}
QMap<QString,QString> ConsumableTree::conditions(){return {{"retail","Original item use conditions"},{"alive","Target is alive"},{"dead","Target is dead"},{"injured","HP is below its limit"},{"petrified","Target is petrified"},{"ordinary","Target is a character"},{"soldier","Target is a Soldier group"},{"hpPercent","HP percentage is below"},{"class","Current class is"},{"fatigue","Fatigue is at least"},{"alignment","Alignment is at least"},{"luck","Luck is at least"},{"army","Used in Army Management"},{"map","Used on the battlefield map"}};}
QJsonObject ConsumableTree::action(const QString &type){return {{"type",type},{"minimum",type=="heal"?100:1},{"maximum",type=="heal"?100:1}};}
QJsonObject ConsumableTree::condition(const QString &type){return {{"type",type},{"value",type=="hpPercent"?50:1},{"not",false}};}
QString ConsumableTree::retailUse(int id){
    if (id >= 1 && id <= 3)
        return "Target is alive and injured. Soldier groups can heal only their surviving members. On the battlefield map, petrified targets are excluded.";
    if (id == 4 || id == 5)
        return "Target is a character, or a Soldier group on the battlefield map.";
    if (id == 6) return "Target is alive and petrified (character or Soldier group).";
    if (id == 7) return "Target is a dead character.";
    if (id == 9) return "Target is a character below level 99.";
    static const QStringList statNames = {"maximum HP", "strength", "vitality", "intelligence",
                                         "mentality", "agility", "dexterity"};
    if (id >= 10 && id <= 16)
        return "Target is a character with " + statNames.at(id - 10) + " below 999.";
    if (id == 17) return "Target is a character with alignment below 100.";
    if (id == 18) return "Target is a character with alignment above 0.";
    if (id == 19) return "Target is a character.";
    return QString("Original use condition for item %1").arg(id);
}
QJsonArray ConsumableTree::retail(int item,const ConsumableEffects &effects){
    QString type;QJsonObject values;
    for(const auto &v:ConsumableEffects::definitions()){const auto d=v.toObject();if(d["itemIds"].toArray().contains(item))values=effects.models()[d["key"].toString()].toObject();}
    if(item<=3)type="heal";else if(item<=5)type="fatigue";else if(item==6)type="cure";else if(item==7)type="revive";else if(item==9)type="level";else if(item==10)type="maxHp";else if(item<=16&&item>=11)type=effectIds()[item-5];else if(item==17||item==18)type="alignment";else if(item==19)type="luck";
    if(type.isEmpty())return {};
    auto a=action(type);a["minimum"]=values.contains("magnitude")?values["magnitude"].toInt():values.value("minimum").toInt(type=="revive"?100:1);
    a["maximum"]=values.contains("magnitude")?values["magnitude"].toInt():values.value("maximum").toInt(type=="revive"?100:1);
    if(type=="revive")a["percent"]=true;
    return {QJsonObject{{"type","if"},{"condition",QJsonObject{{"type","retail"},{"value",item},{"not",false}}},{"then",QJsonArray{a}},{"else",QJsonArray{}}}};
}
QString ConsumableTree::label(const QJsonObject &node) {
    const auto type = node["type"].toString();
    if (type == "if") {
        const auto condition = node["condition"].toObject();
        const auto test = condition["type"].toString();
        const int value = condition["value"].toInt(1);
        auto text = conditions().value(test, "Unknown condition");
        if (test == "retail")
            text = retailUse(value);
        else if (QStringList{"hpPercent", "class", "fatigue", "alignment", "luck"}.contains(test))
            text += " " + QString::number(value) + (test == "hpPercent" ? "%" : "");
        return condition["not"].toBool() ? "IF NOT (" + text + ")" : "IF " + text;
    }

    const auto name = actions().value(type, "Unknown effect");
    if (type == "cure")
        return name;
    const int low = node["minimum"].toInt(1);
    const int high = node["maximum"].toInt(low);
    auto amount = QString::number(low);
    if (high != low)
        amount += " to " + QString::number(high);
    if (node["percent"].toBool())
        amount += "% of maximum HP";
    return name + (name.startsWith("Change ") || type == "fatigue" ? " by " : " · ") + amount;
}
QByteArray ConsumableTree::compile(const QJsonArray &tree){
    QByteArray out;int count=0,actions=0;
    std::function<void(const QJsonArray&,int)> writeNodes=[&](const QJsonArray &nodes,int depth){
        if(depth>8)throw Error("Item effects support up to eight nested conditions");
        for(const auto &v:nodes){if(!v.isObject()||++count>64)throw Error("An item effect supports up to 64 steps");const auto n=v.toObject();const auto type=n["type"].toString();const int at=out.size();out+=QByteArray(16,0);
            if(type=="if"){
                if(!n["condition"].isObject()||!n["then"].isArray()||!n["else"].isArray())throw Error("IF requires a condition, THEN and ELSE");
                const auto c=n["condition"].toObject();const auto ct=c["type"].toString();int op=conditionIds().indexOf(ct);if(op<0)throw Error("Unsupported item condition");
                out[at]=char(128+op);out[at+1]=char(c["not"].toBool());
                put16(out,at+2,integer(c,"value",0,ct=="class"?255:ct=="retail"?19:100,1));
                if(ct=="retail"&&!supported(c["value"].toInt()))throw Error("Choose a supported original item condition");
                writeNodes(n["then"].toArray(),depth+1);const int jump=out.size();out+=QByteArray(16,0);out[jump]=127;put16(out,at+4,out.size()/16);
                writeNodes(n["else"].toArray(),depth+1);put16(out,jump+4,out.size()/16);
            }else{
                if(++actions>16)throw Error("An item effect supports up to sixteen actions");
                int op=effectIds().indexOf(type);if(op<0)throw Error("Unsupported item effect");out[at]=char(op+1);
                int low=(op>=5)?-999:0,high=op>=12?100:999; if(type=="level"){low=1;high=98;}
                if(type=="revive"||n["percent"].toBool()){low=1;high=100;}
                const int lo=integer(n,"minimum",low,high,1),hi=integer(n,"maximum",low,high,lo);if(lo>hi)throw Error("Effect minimum cannot exceed maximum");
                if(n["percent"].toBool()&&type!="heal"&&type!="revive")throw Error("Percentages apply to HP effects only");
                put16(out,at+2,quint16(lo));put16(out,at+4,quint16(hi));out[at+1]=char(n["percent"].toBool());
            }
        }
    };
    writeNodes(tree,0);out+=QByteArray(16,0);return out;
}
void ConsumableTree::validate(const QJsonArray &tree){compile(tree);}
}
