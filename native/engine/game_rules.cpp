#include "engine/game_rules.h"
#include "engine/status_rules.h"
#include <QFile>
#include <QJsonDocument>
#include <cmath>
#include <bit>
namespace lordly {
namespace {
QByteArray resource(const QString &name) {
    QFile f(":/game/tools/" + name);
    if (!f.open(QIODevice::ReadOnly)) throw Error("Missing game-rule source: " + name);
    return f.readAll();
}
QJsonObject catalog() { return QJsonDocument::fromJson(resource("game-rules.json")).object(); }
int cycle(int percent) { return int(std::round(18000. / percent)); }
void alignmentCap(QByteArray &bytes, int cap) {
    put16(bytes,0x156,quint16(cap+1));
    put16(bytes,0x15e,quint16(cap));
    put16(bytes,0x162,quint16(-cap));
    put16(bytes,0x16a,quint16(-cap));
}
void restHealing(QByteArray &bytes, int amount) {
    for(int at:{0x10c,0x138,0x164,0x278}) put16(bytes,at+2,quint16(amount));
    // Keep the full sum until the existing maximum-HP comparison. Truncating
    // before that comparison would wrap a large heal across 65535 to low HP.
    for(int at:{0x114,0x140,0x284}) put32(bytes,at,amount>1 ? 0u : 0x3042ffffu);
}
void period(QByteArray &bytes, const QString &name, int amount) {
    if (name == "fatigue") {
        put16(bytes, 0xac + 2, quint16(amount + 1));
        put16(bytes, 0xb8 + 2, quint16(-amount));
        put16(bytes, 0x100 + 2, quint16(amount + 1));
        put16(bytes, 0x10c + 2, quint16(1 - amount));
    } else {
        put16(bytes, 0x1ac + 2, quint16(amount));
        put16(bytes, 0x1fc + 2, quint16(amount));
    }
}
QByteArray matchup(int width, int amount) {
    QByteArray bytes(width * width, 0);
    for (int i=1;i<width;++i) put8(bytes,i*width+i,quint8(amount));
    for (auto pair : {qMakePair(1,3),qMakePair(2,4),qMakePair(5,6)}) {
        if (pair.second>=width) continue;
        put8(bytes,pair.first*width+pair.second,quint8(-amount));
        put8(bytes,pair.second*width+pair.first,quint8(-amount));
    }
    return bytes;
}
QByteArray resistanceCode(int percent) {
    const quint32 original[]{0x461ac001,0x3c0142c8,0x44812000,0,0x46142081,0x46160000,
                             0x46020002,0x46040003,0x46060002,0x4600008d,0x44101000};
    QByteArray bytes(44,0);
    for(int i=0;i<11;++i) put32(bytes,i*4,original[i]);
    if(percent==100) return bytes;
    // Reuse the original NOP for the first subtraction, preserving the MTC1
    // dependency delay while fitting a complete float constant in the owner.
    const auto bits=std::bit_cast<quint32>(float(percent ? 10000.0/percent : 100.0));
    put32(bytes,0,0x3c010000 | (bits>>16));
    put32(bytes,4,0x34210000 | (bits&65535));
    put32(bytes,8,0x44812000);
    put32(bytes,12,0x461ac001);
    if(!percent) put32(bytes,16,0x46002086); // mov.s f2,f4: no resistance reduction.
    return bytes;
}
QByteArray soldierScale(int percent) {
    const auto bits=std::bit_cast<quint64>(double(percent)/10000.0);
    QByteArray bytes(8,0);put32(bytes,0,quint32(bits>>32));put32(bytes,4,quint32(bits));return bytes;
}
void soldierMinimum(QByteArray &bytes,int value) {
    put32(bytes,0xb4,value>1?0x28620000u|quint32(value):0u);
    put32(bytes,0xb8,value>1?0x54400001u:0x50600001u);
    put32(bytes,0xbc,0x24030000u|quint32(value));
}
QByteArray words(const QByteArray &bytes) {
    QByteArray source;
    for (int i=0;i<bytes.size();i+=4)
        source += ".word 0x" + QByteArray::number(u32(bytes, i), 16) + "\n";
    return source;
}
}
QVector<GameRule> GameRules::definitions() {
    QVector<GameRule> result = {{"deploymentLimit", "deployment", "Maximum deployed squads", "Player squads allowed on a mission at once, up to 20. Retail allows 10; all 30 organized squad slots remain saved. Each squad in a Legion counts separately. Existing story deployments are retained.",1,20,1,10},
            {"reserveXp", "reserve", "XP per reserve member", "Award this much XP once at normal mission completion. 0 disables it. Reaching 100 uses normal class/equipment growth, resets XP to 0, and gains at most one level. Requires an Expansion Pak (8 MiB RAM).",0,100,1,0},
            {"trainingBonus", "trainingXp", "Training XP bonus", "Extra XP for each eligible member after a training win. Manual of Warfare doubles this bonus; the total remains capped at 100. Retail gives 0 bonus.",0,100,1,0},
            {"gothLowDivisor", "goth", "Below 50 Chaos Frame: divisor", "Population bonus = (50 − Chaos Frame) × captured population ÷ this value, rounded down. Base mission Goth is then added.",1,1000,1,25},
            {"gothHighDivisor", "goth", "Above 50 Chaos Frame: divisor", "Population bonus = (Chaos Frame − 50) × captured population ÷ this value, rounded down. At 50, only the base mission Goth is awarded.",1,1000,1,50},
            {"soldierPopulationScale", "soldierPopulation", "Population contribution", "Scale the population × morale contribution before applying the minimum per captured town.",0,400,25,100,true},
            {"soldierMinimum", "soldierMinimum", "Minimum per captured town", "Each captured town contributes at least this many Soldiers, including towns with zero population or morale.",0,20,1,1},
            {"elementAffinity", "elements", "Character elemental affinity", "Applies to elemental weapon hits and spells. Matching the character's element adds this many percentage points; an opposed element subtracts them. Also affects the defender's defense and elemental resistance.",0,25,1,4},
            {"weaponAffinity", "weapons", "Weapon bonus to spells", "Spellcasting bonus from any elemental weapon, including swords and staves. Matching the spell's element adds this many percentage points; opposed weapons subtract them. Normal weapon hits get no bonus from this setting.",0,25,1,2},
            {"resistanceStrength", "resistance", "Resistance strength", "Scale how strongly the final resistance reduces damage. 0% disables that reduction; 100% is retail. Applies to physical and elemental resistance. Fixed-damage and healing rules stay unchanged.",0,400,25,100,true},
            {"dayLength", "clock", "Day length", "Length of 06:00–18:00 relative to retail. Applies to every mission; movement speed stays unchanged.",25,400,25,100,true},
            {"nightLength", "clock", "Night length", "Length of 18:00–06:00 relative to retail. The clock still uses 24 hours and 60 minutes.",25,400,25,100,true},
            {"fatigueRate", "fatigue", "Fatigue buildup", "Higher values shorten the movement cycle between fatigue gains. Existing terrain, night and equipment modifiers remain.",25,400,25,100,true},
            {"recoveryRate", "recovery", "Rest recovery", "Higher values shorten rest cycles for fatigue and HP recovery. Night and Healing Censer bonuses remain. The first cycle keeps its saved progress.",25,400,25,100,true},
            {"restHealing", "restHealing", "HP restored per rest cycle", "HP gained by each eligible member on a rest cycle. Retail restores 1. Zero disables HP healing while fatigue still recovers. Maximum HP and native Soldier-group recovery stages remain.",0,999,1,1},
            {"tacticAuto", "tactics", "Autonomous", "Alignment contribution per kill when using Autonomous (random targeting).",-5,5,1,-1},
            {"tacticStrong", "tactics", "Attack Strongest", "Alignment contribution per kill when targeting greatest current HP.",-5,5,1,0},
            {"tacticLeader", "tactics", "Attack Leader", "Alignment contribution per kill when targeting the leader.",-5,5,1,1},
            {"tacticWeak", "tactics", "Attack Weakest", "Alignment contribution per kill when targeting greatest estimated damage.",-5,5,1,0},
            {"alignmentCap", "alignmentCap", "Maximum alignment change per kill", "Maximum gain or loss after combining the tactic, alignment and level terms. Retail is 5. Zero disables kill-based alignment changes. Alignment itself stays within 0–100; other alignment effects keep their rules.",0,100,1,5}};
    result += statusRuleDefinitions();
    return result;
}
GameRules GameRules::load(const Rom &rom) {
    GameRules result;
    for (const auto &rule : definitions()) result.values_[rule.key] = rule.retail;
    const auto sites = catalog();
    for (auto it=sites.begin();it!=sites.end();++it) {
        const QString name=it.key();
        try {
            rom.requireSupported();
            if (rom.revision()) throw Error("These game rules currently require US revision zero.");
            const auto site=it->toObject();
            auto bytes=rom.bytes().mid(site["offset"].toInt(),site["size"].toInt());
            auto canonical=bytes;
            if(name=="alignmentCap" || name=="restHealing") {
                const bool alignment=name=="alignmentCap";
                const int amount=u16(bytes,alignment ? 0x15e : 0x27a);
                if(amount>(alignment ? 100 : 999)) throw Error("This rule is outside the supported range.");
                if(alignment) alignmentCap(canonical,5); else restHealing(canonical,1);
                if(sha256(canonical)!=site["sha256"].toString().toLatin1())
                    throw Error("This rule's code is unrecognized; its existing bytes will be preserved.");
                auto expected=canonical;
                if(alignment) alignmentCap(expected,amount); else restHealing(expected,amount);
                if(expected!=bytes) throw Error("This rule's checks disagree; its existing code will be preserved.");
                result.values_[name]=amount;
            } else if(name.startsWith("deployment")) {
                const int value=u16(bytes,2);const auto prefix=name=="deployment"?0x24020000u:name=="deploymentMenu"?0x24030000u:0x24040000u;
                if((u32(bytes,0)&0xffff0000u)!=prefix || value<1 || value>30)
                    throw Error("Deployment limit code is unrecognized; its existing value will be preserved.");
                if(name!="deployment" && result.value("deploymentLimit")!=value)
                    throw Error("Deployment limit checks disagree; their existing values will be preserved.");
                result.values_["deploymentLimit"]=value;
            } else if(name=="reserve") {
                const auto hook=rom.bytes().mid(0x139afc,4);
                int amount=0;
                if(bytes!=QByteArray(4096,char(-1))) {
                    amount=int(u32(bytes,8));put32(canonical,8,0);
                    if(bytes.first(8)!="LCRESXP1" || amount<1 || amount>100 ||
                       sha256(canonical)!=site["authoredSha256"].toString().toLatin1() ||
                       hook!=QByteArray::fromHex("0c0794b4") ||
                       sha256(rom.bytes().mid(0x139a10,164))!=site["bootstrapSha256"].toString().toLatin1())
                        throw Error("Reserve XP installation is unrecognized; its bytes will be preserved.");
                } else if(hook!=QByteArray::fromHex("0c0771c8")) throw Error("Mission reward call has an unknown modification.");
                result.values_["reserveXp"]=amount;result.owners_["reserveHook"]=hook;
            } else if(name=="trainingXp") {
                const quint32 word=u32(bytes,0);
                const int value=word==0x00008021?0:int(word&65535);
                if(word!=0x00008021 && ((word&0xffff0000)!=0x24100000 || value>100))
                    throw Error("Training XP rule is unrecognized; its existing code will be preserved.");
                result.values_["trainingBonus"]=value;
            } else if (name=="goth") {
                if(sha256(bytes)!=site["sha256"].toString().toLatin1()) {
                    const int low=u16(bytes,26),high=u16(bytes,50);
                    put16(canonical,26,25);put16(canonical,50,50);
                    if(rom.bytes().mid(0x27c6000,8)=="LCRESXP1" &&
                       sha256(canonical.mid(80))==sites["reserve"].toObject()["bootstrapSha256"].toString().toLatin1())
                        canonical.replace(80,164,QByteArray(164,0));
                    if(sha256(canonical)!=site["authoredSha256"].toString().toLatin1() || low<1 || low>1000 || high<1 || high>1000)
                        throw Error("Goth formula is unrecognized; its existing code will be preserved.");
                    result.values_["gothLowDivisor"]=low;result.values_["gothHighDivisor"]=high;
                }
            } else if(name=="soldierPopulation") {
                bool found=false;
                for(int value=0;value<=400;value+=25) if(bytes==soldierScale(value)) {
                    result.values_["soldierPopulationScale"]=value;found=true;break;
                }
                if(!found) throw Error("Soldier population scale is unrecognized; its existing value will be preserved.");
            } else if(name=="soldierMinimum") {
                soldierMinimum(canonical,1);
                if(sha256(canonical)!=site["sha256"].toString().toLatin1())
                    throw Error("Soldier formula is unrecognized; its existing code will be preserved.");
                bool found=false;
                for(int value=0;value<=20;++value) {auto expected=canonical;soldierMinimum(expected,value);
                    if(bytes==expected) {result.values_["soldierMinimum"]=value;found=true;break;}}
                if(!found) throw Error("Soldier minimum is outside the supported range.");
            } else if (name=="elements" || name=="weapons") {
                const int width=name=="elements"?5:7,amount=qint8(u8(bytes,width+1));
                if(amount<0 || amount>25 || bytes!=matchup(width,amount))
                    throw Error("Element matchup table is unrecognized; its existing bytes will be preserved.");
                result.values_[name=="elements"?"elementAffinity":"weaponAffinity"]=amount;
            } else if(name=="resistance") {
                bool found=false;
                for(int value=0;value<=400;value+=25) if(bytes==resistanceCode(value)) {
                    result.values_["resistanceStrength"]=value;found=true;break;
                }
                if(!found) throw Error("Resistance calculation is unrecognized; its existing code will be preserved.");
            } else if (name == "clock") {
                if (sha256(bytes) != site["sha256"].toString().toLatin1()) {
                    const int night=u16(bytes,66),day=u16(bytes,78);
                    put16(canonical,66,60);put16(canonical,78,60);
                    if (sha256(canonical) != site["authoredSha256"].toString().toLatin1() ||
                        day<15 || day>240 || day%15 || night<15 || night>240 || night%15)
                        throw Error("Clock code is unrecognized; its existing bytes will be preserved.");
                    result.values_["dayLength"]=day*100/60;
                    result.values_["nightLength"]=night*100/60;
                }
            } else if (name=="tactics") {
                const QStringList keys{"tacticAuto","tacticStrong","tacticLeader","tacticWeak"};
                for (int i=0;i<4;++i) {
                    const int value=qint8(u8(bytes,i));
                    if (value < -5 || value > 5) throw Error("Tactic alignment table is outside the supported range.");
                    result.values_[keys[i]]=value;
                }
            } else {
                period(canonical,name,180);
                if (sha256(canonical) != site["sha256"].toString().toLatin1())
                    throw Error("This rule's code is unrecognized; its existing bytes will be preserved.");
                bool found=false;
                for (int value=25;value<=400;value+=25) {
                    auto expected=canonical;period(expected,name,cycle(value));
                    if(expected==bytes) { result.values_[name=="fatigue"?"fatigueRate":"recoveryRate"]=value;found=true;break; }
                }
                if(!found) throw Error("This rule does not use a supported rate.");
            }
            result.owners_[name]=bytes;
        } catch (const Error &e) { result.unavailable_[name]=QString::fromUtf8(e.what()); }
    }
    for(const auto &name:{QString("deploymentMenu"),QString("deploymentMessage")})
        if(!result.unavailable(name).isEmpty()) result.unavailable_["deployment"]=result.unavailable(name);
    // A recognized loader may still point to an unrecognized module. Do not
    // remove that loader while editing an otherwise recognized Goth formula.
    if(!result.unavailable("reserve").isEmpty() && rom.bytes().mid(0x139afc,4)!=QByteArray::fromHex("0c0771c8"))
        result.unavailable_["goth"]=result.unavailable("reserve");
    loadStatusRules(rom,result.values_,result.unavailable_,result.owners_);
    result.originals_=result.values_;
    return result;
}
QJsonObject GameRules::json() const {
    QJsonObject result; for(auto it=values_.begin();it!=values_.end();++it) result[it.key()]=it.value();
    return result;
}
void GameRules::apply(const QJsonObject &settings) {
    for(auto it=settings.begin();it!=settings.end();++it)
        if(!values_.contains(it.key())) throw Error("Unknown game rule: " + it.key());
    auto next=values_;
    for(const auto &rule:definitions()) {
        if(!settings.contains(rule.key)) continue; // Earlier projects preserve newly introduced rules.
        const auto value=settings[rule.key]; const double number=value.toDouble(-9999);
        // Keep older projects readable so their previous 21–30 setting can be
        // corrected with Undo. New UI edits stop at 20; export rejects above 20.
        const int readableMaximum=rule.key=="deploymentLimit"?30:rule.maximum;
        if(!value.isDouble() || !std::isfinite(number) || number!=std::floor(number) ||
           number<rule.minimum || number>readableMaximum || (int(number)-rule.minimum)%rule.step)
            throw Error("Invalid value for " + rule.label);
        if(int(number)!=values_[rule.key] && !unavailable(rule.group).isEmpty())
            throw Error(unavailable(rule.group));
        next[rule.key]=int(number);
    }
    values_=next;
}
QVector<RuleBuild> GameRules::builds() const {
    QVector<RuleBuild> result;const auto sites=catalog();
    for(auto it=sites.begin();it!=sites.end();++it) {
        const auto name=it.key(); bool changed=false;
        for(const auto &rule:definitions()) if(rule.group==name) changed|=value(rule.key)!=loaded(rule.key);
        if(name.startsWith("deployment")) changed|=value("deploymentLimit")!=loaded("deploymentLimit");
        if(name=="goth") changed|=value("reserveXp")!=loaded("reserveXp");
        if(!changed) continue;
        if(!unavailable(name).isEmpty()) throw Error(unavailable(name));
        const auto site=it->toObject();
        RuleBuild build{name,site["offset"].toInt(),site["size"].toInt(),quint32(site["address"].toInteger()),{},owners_[name],{},name=="tactics" || name=="elements" || name=="weapons" || name=="soldierPopulation"};
        if(name=="alignmentCap" || name=="restHealing") {
            if(name=="alignmentCap") alignmentCap(build.expected,value(name)); else restHealing(build.expected,value(name));
            build.source=words(build.expected);
        } else if(name.startsWith("deployment")) {
            put16(build.expected,2,quint16(value("deploymentLimit")));build.source=words(build.expected);
        } else if(name=="reserve") {
            if(value("reserveXp")) {
                build.source=QString(".set RESERVE_XP,%1\n").arg(value("reserveXp")).toLatin1()+resource("reserve-xp.s");
                build.expected.clear();
            } else {build.expected=QByteArray(4096,char(-1));build.data=true;}
        } else if(name=="trainingXp") {
            const auto bonus=value("trainingBonus");
            put32(build.expected,0,bonus?0x24100000u|quint32(bonus):0x00008021u);
            build.source=words(build.expected);
        } else if(name=="goth") {
            const int low=value("gothLowDivisor"),high=value("gothHighDivisor");
            auto goth=resource("goth-reward.s");
            if(value("reserveXp")) goth.replace(".org 0xf4",resource("reserve-bootstrap.s")+"\n.org 0xf4");
            build.source=low==25 && high==50 && !value("reserveXp") ? resource("goth-retail.s") :
                QString(".set LOW_DIVISOR,%1\n.set HIGH_DIVISOR,%2\n").arg(low).arg(high).toLatin1()+goth;
            build.expected.clear();
        } else if(name=="soldierPopulation") {
            build.expected=soldierScale(value("soldierPopulationScale"));
        } else if(name=="soldierMinimum") {
            soldierMinimum(build.expected,value("soldierMinimum"));build.source=words(build.expected);
        } else if(name=="elements" || name=="weapons") {
            build.expected=matchup(name=="elements"?5:7,value(name=="elements"?"elementAffinity":"weaponAffinity"));
        } else if(name=="resistance") {
            build.expected=resistanceCode(value("resistanceStrength"));build.source=words(build.expected);
        } else if(name=="clock") {
            const int day=value("dayLength")*60/100,night=value("nightLength")*60/100;
            if(day==60 && night==60) build.source=resource("clock-retail.s");
            else build.source=QString(".set DAY_SECONDS,%1\n.set NIGHT_SECONDS,%2\n").arg(day).arg(night).toLatin1()+resource("clock.s");
            build.symbols={{"CLOCK_NEXT_DATE",0x8016c5bc}};
            build.expected.clear();
        } else if(name=="tactics") {
            const QStringList keys{"tacticAuto","tacticStrong","tacticLeader","tacticWeak"};
            for(int i=0;i<4;++i) put8(build.expected,i,quint8(value(keys[i])));
        } else {
            period(build.expected,name,cycle(value(name=="fatigue"?"fatigueRate":"recoveryRate")));
            build.source=words(build.expected);
        }
        if(name=="goth" || name=="reserve") build.symbols={{"RESERVE_INVALIDATE_I",0x800900c0},
            {"RESERVE_INVALIDATE_D",0x80090010},{"RESERVE_ENTRY",0x804c0010},
            {"RESERVE_ITEM",0x801dc720},{"RESERVE_GROW",0x8016eba4}};
        result<<build;
    }
    if(value("reserveXp")!=loaded("reserveXp")) {
        if(!unavailable("reserve").isEmpty())throw Error(unavailable("reserve"));
        const auto expected=QByteArray::fromHex(value("reserveXp")?"0c0794b4":"0c0771c8");
        result<<RuleBuild{"reserveHook",0x139afc,4,0x801e53bc,words(expected),expected,{},false};
    }
    result += buildStatusRules(*this,owners_);
    return result;
}
}
