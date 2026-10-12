#include "engine/game_rules.h"
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
QByteArray words(const QByteArray &bytes) {
    QByteArray source;
    for (int i=0;i<bytes.size();i+=4)
        source += ".word 0x" + QByteArray::number(u32(bytes, i), 16) + "\n";
    return source;
}
}
QVector<GameRule> GameRules::definitions() {
    return {{"elementAffinity", "elements", "Element affinity", "Same element adds this many percentage points; an opposed element subtracts it. Shared by attack, defense, magic context and elemental resistance.",0,25,1,4},
            {"weaponAffinity", "weapons", "Magic weapon affinity", "Same-element weapon bonus in percentage points; opposed weapons subtract the same amount. Physical weapons receive no bonus.",0,25,1,2},
            {"resistanceStrength", "resistance", "Resistance strength", "Scale how strongly the final resistance reduces damage. 0% disables that reduction; 100% is retail. Applies to physical and elemental resistance. Fixed-damage and healing rules stay unchanged.",0,400,25,100,true},
            {"dayLength", "clock", "Day length", "Length of 06:00–18:00 relative to retail. Applies to every mission; movement speed stays unchanged.",25,400,25,100,true},
            {"nightLength", "clock", "Night length", "Length of 18:00–06:00 relative to retail. The clock still uses 24 hours and 60 minutes.",25,400,25,100,true},
            {"fatigueRate", "fatigue", "Fatigue buildup", "Higher values shorten the movement cycle between fatigue gains. Existing terrain, night and equipment modifiers remain.",25,400,25,100,true},
            {"recoveryRate", "recovery", "Rest recovery", "Higher values shorten rest cycles for fatigue and HP recovery. Night and Healing Censer bonuses remain. The first cycle keeps its saved progress.",25,400,25,100,true},
            {"tacticAuto", "tactics", "Autonomous", "Alignment contribution per kill when using Autonomous (random targeting).",-5,5,1,-1},
            {"tacticStrong", "tactics", "Attack Strongest", "Alignment contribution per kill when targeting greatest current HP.",-5,5,1,0},
            {"tacticLeader", "tactics", "Attack Leader", "Alignment contribution per kill when targeting the leader.",-5,5,1,1},
            {"tacticWeak", "tactics", "Attack Weakest", "Alignment contribution per kill when targeting greatest estimated damage.",-5,5,1,0}};
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
            if (name=="elements" || name=="weapons") {
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
        if(!value.isDouble() || !std::isfinite(number) || number!=std::floor(number) ||
           number<rule.minimum || number>rule.maximum || (int(number)-rule.minimum)%rule.step)
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
        if(!changed) continue;
        if(!unavailable(name).isEmpty()) throw Error(unavailable(name));
        const auto site=it->toObject();
        RuleBuild build{name,site["offset"].toInt(),site["size"].toInt(),quint32(site["address"].toInteger()),{},owners_[name],{},name=="tactics" || name=="elements" || name=="weapons"};
        if(name=="elements" || name=="weapons") {
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
        result<<build;
    }
    return result;
}
}
