#include "engine/status_rules.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <bit>

namespace lordly {
namespace {
QJsonObject sites() {
    QFile file(":/game/tools/status-effects.json");
    if (!file.open(QIODevice::ReadOnly)) throw Error("Missing status effects definitions");
    return QJsonDocument::fromJson(file.readAll()).object();
}
QString keyFor(const QString &name) {
    if (name=="statusPoison") return "poisonStrength";
    if (name=="statusPowerUp") return "powerUpStrength";
    if (name=="statusPowerDown") return "powerDownStrength";
    if (name=="statusParalysis") return "paralysisRecoveryChance";
    return "sleepRecoveryChance";
}
bool strength(const QString &name) {
    return name=="statusPoison" || name=="statusPowerUp" || name=="statusPowerDown";
}
QByteArray amount(QByteArray bytes, const QString &name, int percent) {
    if (name=="statusPoison") {
        const auto bits=std::bit_cast<quint64>(double(percent)/100.0);
        put32(bytes,0,quint32(bits>>32)); put32(bytes,4,quint32(bits));
    } else {
        float value=float(percent)/100.0f;
        if (name=="statusPowerDown") value=-value;
        const auto bits=std::bit_cast<quint32>(value);
        put16(bytes,2,quint16(bits>>16)); put16(bytes,6,quint16(bits));
    }
    return bytes;
}
struct Recovery { QByteArray canonical; QJsonObject layout; int chance; };
QByteArray recoveryBytes(QByteArray bytes, const QJsonObject &layout, int chance) {
    if (chance < -1 || chance > 100) throw Error("Recovery chance must be Luck-based or 0–100%");
    const int load=layout["load"].toInt(), compare=layout["compare"].toInt();
    // All qualified owners use s2 for Luck/threshold, s0 for the sampled roll.
    put32(bytes,load,chance<0 ? quint32(layout["loadWord"].toInteger()) : 0x24120000u|quint32(chance));
    put32(bytes,compare,chance<0 ? quint32(layout["compareWord"].toInteger()) : 0x0212902bu);
    put32(bytes,compare+4,quint32(layout["branchWord"].toInteger()) ^ (chance<0 ? 0u : 0x04000000u));
    return bytes;
}
Recovery decodeRecovery(const QByteArray &bytes, const QJsonObject &site) {
    for (const auto &entry:site["layouts"].toArray()) {
        const auto layout=entry.toObject();
        const auto load=u32(bytes,layout["load"].toInt());
        int chance=-1;
        if (load!=quint32(layout["loadWord"].toInteger())) {
            if ((load&0xffff0000u)!=0x24120000u || (load&65535)>100) continue;
            chance=int(load&65535);
        }
        const auto canonical=recoveryBytes(bytes,layout,-1);
        if (sha256(canonical)==layout["sha256"].toString().toLatin1() &&
            recoveryBytes(canonical,layout,chance)==bytes) return {canonical,layout,chance};
    }
    throw Error("Status recovery code is unrecognized; its existing bytes will be preserved");
}
QByteArray clamps(QByteArray bytes, const QJsonObject &site, bool fullWidth) {
    for (const auto &at:site["narrowingOffsets"].toArray()) {
        put32(bytes,at.toInt(),fullWidth ? 0x00401821u : 0x00021400u); // addu v1,v0,zero / sll v0,v0,16
        put32(bytes,at.toInt()+4,fullWidth ? 0u : 0x00021c03u);
    }
    return bytes;
}
QByteArray words(const QByteArray &bytes) {
    QByteArray source;
    for(int at=0;at<bytes.size();at+=4) source+=".word 0x"+QByteArray::number(u32(bytes,at),16)+"\n";
    return source;
}
}
QVector<GameRule> statusRuleDefinitions() {
    return {{"poisonStrength","statusPoison","Attribute penalty","Reduces baseline STR, VIT, INT, MEN, AGI, DEX and Luck while poisoned. One penalty; repeated Poison does not stack. HP is unaffected.",0,100,1,10,true},
            {"powerUpStrength","statusPowerUp","Attribute gain per application","Adds this percentage of baseline attributes for each Power Up application. Stacks in battle; HP and permanent attributes are unchanged.",0,100,1,10,true},
            {"powerDownStrength","statusPowerDown","Attribute loss per application","Subtracts this percentage of baseline attributes for each Power Down application. Stacks in battle; HP and permanent attributes are unchanged.",0,100,1,10,true},
            {"sleepRecoveryChance","statusSleep","Recovery chance","Luck-based uses min(Luck + 1, 100)%. A fixed chance applies at each lost action and on an eligible nonlethal physical hit. 0% disables natural waking; cures still work.",-1,100,1,-1,true},
            {"paralysisRecoveryChance","statusParalysis","Recovery chance per lost action","Luck-based uses min(Luck + 1, 100)%. A fixed chance applies at each lost action. Recovery still spends that action. 0% disables natural recovery; cures still work.",-1,100,1,-1,true}};
}
void loadStatusRules(const Rom &rom, QMap<QString,int> &values, QMap<QString,QString> &unavailable,
                     QMap<QString,QByteArray> &owners) {
    const auto catalog=sites();
    for(auto it=catalog.begin();it!=catalog.end();++it) {
        const auto name=it.key(), group=name=="statusWake" ? QString("statusSleep") : name;
        try {
            rom.requireSupported();
            if(rom.revision()) throw Error("Status rules require US revision zero");
            const auto site=it->toObject();
            const auto bytes=rom.bytes().mid(site["offset"].toInt(),site["size"].toInt());
            if(bytes.size()!=site["size"].toInt()) throw Error("Status rule is outside the ROM");
            if(strength(name)) {
                const auto canonical=amount(bytes,name,10);
                if(sha256(canonical)!=site["sha256"].toString().toLatin1())
                    throw Error("Status strength code is unrecognized; its bytes will be preserved");
                bool found=false;
                for(int percent=0;percent<=100;++percent) if(amount(canonical,name,percent)==bytes) {
                    values[keyFor(name)]=percent; found=true; break;
                }
                if(!found) throw Error("Status strength is outside the supported percentage range");
            } else if(name=="statusAttributes") {
                const auto canonical=clamps(bytes,site,false);
                if(sha256(canonical)!=site["sha256"].toString().toLatin1() ||
                    (bytes!=canonical && bytes!=clamps(canonical,site,true)))
                    throw Error("Status attribute calculation is unrecognized; its bytes will be preserved");
            } else {
                const auto decoded=decodeRecovery(bytes,site);
                if(name=="statusWake" && values[keyFor(name)]!=decoded.chance)
                    throw Error("Sleep's action and physical-hit recovery settings disagree; existing code is preserved");
                values[keyFor(name)]=decoded.chance;
            }
            owners[name]=bytes;
        } catch(const Error &error) { unavailable[group]=QString::fromUtf8(error.what()); }
    }
    if(!unavailable.value("statusAttributes").isEmpty())
        for(const auto &name:{"statusPoison","statusPowerUp","statusPowerDown"}) unavailable[name]=unavailable["statusAttributes"];
}
QVector<RuleBuild> buildStatusRules(const GameRules &rules, const QMap<QString,QByteArray> &owners) {
    QVector<RuleBuild> result; const auto catalog=sites();
    const bool strengthChanged=rules.value("poisonStrength")!=rules.loaded("poisonStrength") ||
        rules.value("powerUpStrength")!=rules.loaded("powerUpStrength") ||
        rules.value("powerDownStrength")!=rules.loaded("powerDownStrength");
    for(auto it=catalog.begin();it!=catalog.end();++it) {
        const auto name=it.key(), key=keyFor(name), group=name=="statusWake" ? QString("statusSleep") : name;
        const bool attributes=name=="statusAttributes";
        if(attributes ? !strengthChanged : rules.value(key)==rules.loaded(key)) continue;
        if(!rules.unavailable(group).isEmpty()) throw Error(rules.unavailable(group));
        const auto site=it->toObject(); auto bytes=owners.value(name);
        if(bytes.size()!=site["size"].toInt()) throw Error("Missing authenticated status owner: "+name);
        if(strength(name)) bytes=amount(bytes,name,rules.value(key));
        else if(attributes) bytes=clamps(bytes,site,rules.value("poisonStrength")!=10 ||
            rules.value("powerUpStrength")!=10 || rules.value("powerDownStrength")!=10);
        else bytes=recoveryBytes(bytes,decodeRecovery(bytes,site).layout,rules.value(key));
        const bool data=name=="statusPoison";
        result.append({name,site["offset"].toInt(),site["size"].toInt(),quint32(site["address"].toInteger()),
                       data ? QByteArray() : words(bytes),bytes,{},data});
    }
    return result;
}
QByteArray canonicalWeaponRecovery(const QByteArray &bytes, int *chance) {
    const auto decoded=decodeRecovery(bytes,sites()["statusWake"].toObject());
    if(chance) *chance=decoded.chance;
    return decoded.canonical;
}
QByteArray withWeaponRecovery(const QByteArray &bytes, int chance) {
    const auto decoded=decodeRecovery(bytes,sites()["statusWake"].toObject());
    return recoveryBytes(bytes,decoded.layout,chance);
}
}
