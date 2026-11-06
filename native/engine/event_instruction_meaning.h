#pragma once
#include "engine/event_helper_names.h"
#include <QByteArray>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <array>
#include <functional>

namespace lordly {

// Presentation inputs come from the current program's conservative dataflow.
// A register has no permanent game meaning, and -1 means no proved constant.
struct EventInstructionValue {
    QString text;
    int literal = -1;
};

class EventInstructionMeaning {
  public:
    struct Description { QString title, detail; };
    using Values = std::array<EventInstructionValue, 8>;
    using Inputs = std::function<Values(int)>;

    // Construct once per projection. Whole-program authentication, rather than
    // matching an isolated instruction, qualifies contextual retail annotations.
    EventInstructionMeaning(const QJsonObject &catalog, const QByteArray &bytes,
                            int row, bool originalContext = false)
        : catalog_(catalog), bytes_(bytes), row_(row), names_(sharedNames(catalog)) {
        const auto info = catalog_["rows"].toObject()[QString::number(row)].toObject();
        original_ = originalContext && !info.isEmpty() &&
            QCryptographicHash::hash(bytes_, QCryptographicHash::Sha256).toHex() ==
                info["sha256"].toString().toLatin1();
        for (const auto &v : info["steps"].toArray()) {
            const auto step = v.toArray();
            instructionOffsets_.insert(step[0].toInt());
            if (original_)
                originalSteps_[step[0].toInt()] = step[1].toString();
        }
        if (original_)
            for (const auto &v : info["handlers"].toArray()) {
                const auto h=v.toObject();originalHandlers_[h["entry"].toInt()]=h["title"].toString();
            }
    }

    // offsets must be the caller's already-owned instruction block. The final
    // effect is the title. Technical instruction detail stays in Raw.
    // This never changes bytes, control-flow targets, or edit permissions.
    Description describe(const QVector<int> &offsets, const Inputs &inputs, int group = -1) const {
        for (auto it=offsets.crbegin();it!=offsets.crend();++it) {
            const int at=*it;
            if (at < 0 || at + 1 >= bytes_.size() || (at & 1)) continue;
            const auto values=describedInputs(at,inputs ? inputs(at) : Values{});
            auto value = instruction(at, values, group);
            const auto intent=originalIntent(at,values);
            if(!intent.isEmpty())value.title=intent;
            if (value.title.isEmpty()) continue;
            return {value.title,{}};
        }
        return {};
    }

  private:
    QJsonObject catalog_;
    QByteArray bytes_;
    int row_;
    bool original_ = false;
    QMap<int, QString> originalSteps_;
    QMap<int, QString> originalHandlers_;
    QSet<int> instructionOffsets_;
    const QMap<QString, QMap<int, QString>> &names_;

    static const QMap<QString, QMap<int, QString>> &sharedNames(const QJsonObject &catalog) {
        // The embedded semantic catalog is immutable for the application run.
        // Keep its large name index shared between all row projections.
        static const auto entries = [&catalog] {
            QMap<QString, QMap<int, QString>> result;
            for (const auto &v : catalog["names"].toArray()) {
                const auto a = v.toArray();
                result[a[0].toString() + "/" + a[1].toString()][a[2].toInt()] = a[3].toString();
            }
            return result;
        }();
        return entries;
    }

    int byte(int at) const { return quint8(bytes_.at(at)); }
    QString named(const QString &kind, int id, const QString &scope = "global") const {
        return names_.value(kind + "/" + scope).value(id);
    }
    QString reference(const QString &kind, int id, const QString &fallback,
                      const QString &scope = "global") const {
        const auto name = kind == "scene" && !original_ ? QString{} : named(kind, id, scope);
        return name.isEmpty() ? QString("%1 %2").arg(fallback).arg(id) : name;
    }
    QString request(int id) const {
        return catalog_["requests"].toObject()[QString::number(id)].toObject()["name"]
            .toString(QString("Game operation %1").arg(id));
    }
    QString property(int id, int group) const {
        if (id == 0xfd) {
            if (group == 1 || group == 2) return "Contacted town (zero-based)";
            if (group == 3) return "Encountered squad";
            if (group == 4) return "Defeated squad";
        }
        if (id == 0xfe) {
            if (group == 1 || group == 2) return "Visiting squad";
            if (group == 3) return "Contacting squad";
        }
        if (id == 0xf8 && (group == 1 || group == 2)) return "Contacted town (one-based)";
        const auto name = catalog_["properties"].toObject()[QString::number(id)].toObject()["name"].toString();
        return name.isEmpty() ? QString("Game property %1").arg(id) : name;
    }
    static QString text(const EventInstructionValue &value, int reg) {
        if (value.literal >= 0) return QString::number(quint16(value.literal));
        return value.text.isEmpty() ? QString("working value %1").arg(reg + 1) : value.text;
    }
    static QString low(const EventInstructionValue &value, int reg) {
        if (value.literal >= 0) return QString::number(value.literal & 255);
        return "low byte of " + text(value, reg);
    }
    static QString index(const EventInstructionValue &value, int reg) {
        return value.literal >= 0 ? QString::number(qint16(value.literal)) : text(value, reg);
    }
    QString typed(const EventInstructionValue &value, int reg, const QString &kind,
                  const QString &fallback, bool lowByte = false) const {
        if (value.literal >= 0)
            return reference(kind, lowByte ? value.literal & 255 : quint16(value.literal), fallback);
        return lowByte ? low(value, reg) : text(value, reg);
    }
    static bool comparison(int op) {
        if (op < 0xa0) return false;
        switch (op & 15) { case 1: case 2: case 3: case 6: case 7: case 11:
                          case 12: case 13: case 14: case 15: return true; default: return false; }
    }
    static QString relation(int op) {
        switch (op >> 4) { case 10: return "="; case 11: return "≠"; case 12: return ">";
                          case 13: return "<"; case 14: return "≥"; default: return "≤"; }
    }

    QString annotationRole(int at,int reg) const {
        if(!original_)return {};
        const auto source=originalSteps_.value(at);
        const QRegularExpression pattern(QString("\\bR%1\\s*\\(").arg(reg));
        const auto match=pattern.match(source);
        if(!match.hasMatch())return {};
        const int start=match.capturedEnd();int depth=1,end=start;
        for(;end<source.size() && depth;++end){
            if(source[end]=='(')++depth;else if(source[end]==')')--depth;
        }
        if(depth)return {};
        const auto role=source.mid(start,end-start-1).trimmed();
        // Caller-frame labels describe preservation, not a game-domain value.
        // Do not turn those into a permanent register name.
        if(role.contains("caller",Qt::CaseInsensitive) || role.contains('@') ||
           role.contains(QRegularExpression("\\bR[0-7]\\b")))return {};
        return role;
    }
    QString shortInputRole(int at,int reg) const {
        const auto role=annotationRole(at,reg);
        const auto lower=role.toLower();
        if(lower.contains("birthday guest speech"))return "selected birthday guest speech";
        if(lower.contains("birthday guest identity"))return "selected birthday guest identity";
        if(lower.contains("birthday guest current class"))return "selected birthday guest current class";
        if(lower.contains("temporary") && lower.contains("flag"))return "current transient gameplay flag";
        if(lower.contains("transient") && lower.contains("flag"))return "current transient gameplay flag";
        if(lower.contains("world road"))return "current world road";
        if(lower.contains("world location") || lower=="map location")return "current world location";
        if(lower.contains("consumable inventory slot"))return "candidate consumable inventory slot";
        if(lower.contains("equipment inventory slot"))return "candidate equipment inventory slot";
        if(lower.contains("inventory slot"))return "candidate inventory slot";
        if(lower.contains("army character slot"))return "selected army character slot";
        if(lower.contains("character slot"))return "selected character slot";
        if(lower.contains("formation position") || lower.contains("member position"))return "selected squad member position";
        if(lower.contains("character identity"))return "selected character identity";
        if(lower.contains("current class"))return "selected current class";
        if(lower.contains("chaos frame"))return "prepared Chaos Frame";
        if(lower.contains("five birthday guest slots"))return "current birthday guest slot";
        if(role.size()<=60 && !role.isEmpty() && !role.contains("context:") && !role.contains("input to"))return role;
        return {};
    }
    Values describedInputs(int at,Values r) const {
        const int op=byte(at),arg=byte(at+1),d=op&7,i=arg&7,bank=arg>>3;
        const auto supply=[&](int reg,const QString &role){
            if(r[reg].literal<0 && r[reg].text.isEmpty())r[reg].text=role;
        };
        // Reviewed roles are attached to this exact instruction and only after
        // whole-program authentication. Native consumer roles below remain
        // valid when an operand, branch, or unrelated instruction is edited.
        for(int reg=0;reg<8;++reg)supply(reg,shortInputRole(at,reg));
        if((op>=0x60 && op<=0x67) || (op>=0x70 && op<=0x87)){
            const bool alternate=op>=0x78;
            const bool write=(op>=0x70 && op<=0x77) || (op>=0x80 && op<=0x87);
            if(!alternate && arg>=0xe4){
                if(write)supply(d,"prepared "+property(arg,-1));
            }else if(arg<0xe4){
                if(!alternate){
                    if(bank==1){supply(i,"selected squad member position");supply(7,"selected squad");}
                    else if(bank>=2 && bank<=6)supply(i,"selected army character slot");
                    else if(bank>=7 && bank<=11)supply(i,"selected opposing character slot");
                    else if(bank==12)supply(i,"selected story flag");
                    else if(bank==13)supply(i,"selected squad");
                    else if(bank>=14 && bank<=17)supply(i,"selected Soldier slot");
                    else if(bank==18)supply(i,"selected dialogue-control field");
                    else if(bank>=19 && bank<=21)supply(write?d:i,"selected town");
                    else if(bank==22)supply(i,"selected world location");
                    else if(bank==23)supply(i,"selected world road");
                    else if(bank==24 || bank==25)supply(i,"current scene actor slot");
                    else if(bank==26)supply(i,"selected cutscene text substitution");
                    else if(bank==27)supply(i,"current transient gameplay flag");
                    else if(bank==28)supply(i,"selected army character slot");
                }else{
                    if(bank>=2 && bank<=4)supply(i,"candidate consumable inventory slot");
                    if(bank>=6 && bank<=8)supply(i,"candidate equipment inventory slot");
                    if((write && bank==9) || (!write && bank==11))supply(i,"selected character slot");
                    if(write && bank==10)supply(i,"selected world location");
                }
                if(write){
                    const auto definitions=catalog_["banks"].toObject()[alternate?"altSetters":"setters"].toArray();
                    if(bank<definitions.size() && definitions[bank].toObject()["status"]=="verified")
                        supply(d,"prepared "+definitions[bank].toObject()["name"].toString());
                }
            }
        }
        return r;
    }
    QString originalIntent(int at,const Values &r) const {
        if(!original_)return {};
        const auto source=originalSteps_.value(at);
        const int op=byte(at),arg=byte(at+1);
        if(op==0x6f){
            static const QRegularExpression greeting("birthday dialogue entry([0-9]+) for ([^;]+);");
            const auto speaker=greeting.match(source);
            if(speaker.hasMatch()&&speaker.captured(1).toInt()==arg)
                return "Choose the birthday greeting for "+speaker.captured(2);
        }
        if(op>=0x98&&op<=0x9f&&arg==0x41){
            if(source.startsWith("Increment birthday-period ordinal"))return "Advance to the next birthday period";
            if(source.startsWith("Advance to the next of128 transient gameplay flags"))return "Check the next temporary gameplay flag";
            if(source.startsWith("Advance ")&&source.endsWith("consumable-slot index"))return "Check the next consumable inventory slot";
            if(source.startsWith("Advance ")&&source.endsWith("equipment-slot index"))return "Check the next equipment inventory slot";
        }
        static const QRegularExpression grant("^Initial (.+?) grant #(\\d+): (.+)$");
        const auto opening=grant.match(source);
        if(opening.hasMatch()){
            const auto item=opening.captured(1),instruction=opening.captured(3);
            const bool equipment=op==0x99 || (op>=0x80 && op<=0x87 && (arg>>3)>=6) || instruction.contains("equipment");
            if(op==0x99 || op==0x9a)return "Advance the empty-slot inventory search for "+item;
            if(op==0x87)return "Store "+item+" in the selected empty inventory slot";
            if(op==0x86 || op==0x85){
                static const QRegularExpression quantity("quantity\\s*(\\d+)");
                const auto count=quantity.match(instruction);
                if(count.hasMatch())return QString("Initialize %1 copies of %2 · %3").arg(
                    op==0x86?"owned":equipment?"equipped":"carried",item,count.captured(1));
            }
        }
        if(op==0x68 && source.contains("predicate result")){
            const auto purpose=source.section(" for ",1);
            if(!purpose.isEmpty())return QString("Return %1 for %2").arg(arg?"match":"no match",purpose);
        }
        if((op==5 || op==6)){
            QString purpose;
            if(source.contains("birthday-period"))purpose="birthday-period calculation";
            else if(source.contains("Angel Knight or Seraph"))purpose="Angel Knight / Seraph class check";
            else if(source.contains("birthday") && source.contains("identity"))purpose="birthday-guest identity lookup";
            if(purpose.isEmpty()){
                static const QRegularExpression helper("helper @(0x[0-9A-Fa-f]+)");
                const auto match=helper.match(source);
                if(match.hasMatch())for(const auto &annotation:originalSteps_)
                    if(annotation.contains("helper @"+match.captured(1))){
                        if(annotation.contains("birthday-period")){purpose="birthday-period calculation";break;}
                        if(annotation.contains("birthday") && annotation.contains("identity")){purpose="birthday-guest identity lookup";break;}
                    }
            }
            if(!purpose.isEmpty())return (op==5?"Preserve caller context for ":"Restore caller context after ")+purpose;
            auto handler=originalHandlers_.upperBound(at);
            if(handler!=originalHandlers_.begin()){
                --handler;
                if(handler.value().contains("prepare five birthday guests") && (arg&7)==1)
                    return op==5?"Preserve the birthday guest slot during the character search":"Restore the birthday guest slot after the character search";
            }
            if(op==5 && r[arg&7].literal>=0)return {};
        }
        if(op==0x6f && source.contains("fallback birthday speech"))return QString("Select fallback birthday speech entry %1").arg(arg);
        if(op==0x6f && source.contains("world-map update stream"))return "Start the mission-completion world-map update list";
        if(op==0x6f && source.contains("world reveal-stream position"))return "Start the conditional world-map story update list";
        return {};
    }

    struct OperandRole { QString label, kind; };
    OperandRole consumerRole(int at, int reg) const {
        // Follow only consecutive preparation instructions. A load whose value
        // is replaced, branched away, or passed through an unknown call cannot
        // borrow a later consumer's domain. Operand roles are current-byte facts.
        int r0=-1;
        for (int pc = at + 2; pc < bytes_.size() && pc <= at + 12 && instructionOffsets_.contains(pc); pc += 2) {
            const int op = byte(pc), arg = byte(pc + 1), d = op & 7, i = arg & 7, bank = arg >> 3;
            if ((op >= 0x68 && op <= 0x6f) || (op >= 0x88 && op <= 0x8f)) {
                if (d == reg && op <= 0x6f) return {};
                if(d==0){
                    if(op==0x68)r0=arg;
                    else if(r0>=0)r0=(r0&255)|(arg<<8);
                }
                continue;
            }
            if ((op >= 0x60 && op <= 0x67) || (op >= 0x70 && op <= 0x87)) {
                const bool write = (op >= 0x70 && op <= 0x77) || (op >= 0x80 && op <= 0x87);
                const bool alternate = op >= 0x78;
                if (!alternate && arg >= 0xe4)
                    return write && d == reg ? OperandRole{"new " + property(arg, -1), {}} : OperandRole{};
                if (alternate && arg >= 0xe5) return {};
                if (!alternate && bank == 1 && reg == 7) return {"squad", {}};
                if (i == reg) {
                    if (!alternate) {
                        if (bank == 1) return {"member position", {}};
                        if ((bank >= 2 && bank <= 11) || bank == 28) return {"character slot", {}};
                        if (bank == 12) return {"story flag", "flag"};
                        if (bank == 13) return {"squad", {}};
                        if (bank >= 14 && bank <= 17) return {"Soldier slot", {}};
                        if (bank == 18) return {"dialogue control", {}};
                        if (bank >= 19 && bank <= 21) return write ? OperandRole{} : OperandRole{"town", {}};
                        if (bank == 22) return {"world location", "world-location"};
                        if (bank == 23) return {"world road", "world-road"};
                        if (bank == 24 || bank == 25) return {"scene actor slot", {}};
                        if (bank == 26) return {"scene translation slot", {}};
                        if (bank == 27) return {"temporary gameplay flag", "transient-game-state"};
                    } else {
                        if (!write && bank == 1) return {"consumable", "consumable"};
                        if (!write && bank == 5) return {"equipment", "equipment"};
                        if ((bank >= 2 && bank <= 4) || (bank >= 6 && bank <= 8)) return {"inventory slot", {}};
                        if ((write && bank == 9) || (!write && bank == 11)) return {"character slot", {}};
                        if (write && bank == 10) return {"world location", "world-location"};
                        if (!write && bank == 12) return {"build-stamp mask", {}};
                    }
                }
                if (write && d == reg) {
                    if (!alternate) {
                        if (bank == 1) return {"squad member ID", {}};
                        if (bank == 12) return {"story flag state", {}};
                        if (bank == 13) return {"squad flags", {}};
                        if (bank >= 19 && bank <= 21) return {"town", {}};
                        if (bank == 22) return {"world-location state", {}};
                        if (bank == 23) return {"road state", {}};
                        if (bank == 24 || bank == 25) return {"scene actor class", "identity-class"};
                        if (bank == 26) return {"scene translation value", {}};
                        if (bank == 27) return {"temporary gameplay flag state", {}};
                    } else {
                        if (bank == 2 || bank == 6) return {"owned item count", {}};
                        if (bank == 3) return {"consumable", "consumable"};
                        if (bank == 7) return {"equipment", "equipment"};
                        if (bank == 4 || bank == 8) return {"assigned item count", {}};
                        if (bank == 9) return {"character side (0 army, 1 opposing)", {}};
                        if (bank == 10) return {"mission-marker state", {}};
                    }
                }
                return {};
            }
            if (comparison(op)) {
                const bool single = (op & 15) >= 12;
                if (reg == (op & 3) || (!single && reg == ((op >> 2) & 3))) return {"comparison value", {}};
                return {};
            }
            switch (op) {
            case 2: case 3:
                if (reg == 0) return {"shared-logic destination byte", {}};
                if(r0>=0){
                    const int target=quint16((pc+2)/2+(arg<<8)+(r0&255))*2;
                    const auto helper=EventHelperNames::match(bytes_,instructionOffsets_,target);
                    if(helper.kind==EventHelperNames::Kind::IdentityFamily){
                        if(reg==4)return {"candidate character identity","identity-class"};
                        if(reg==5)return {"required character identity","identity-class"};
                    }else if(helper.known() && reg==4)return {"class to check","identity-class"};
                }
                break;
            case 0x10: if (reg == 0) return {"cutscene", "scene"}; break;
            case 0x11: if (reg == 0) return {"next game operation", {}}; break;
            case 0x12: if (reg == 0) return {"flag reset interval", {}}; break;
            case 0x15: if (reg < 3) return {reg == 0 ? "history year" : reg == 1 ? "history month" : "history day", {}}; break;
            case 0x16:
                if (reg == 0 && !(arg >= 5 && arg <= 8)) return arg & 128 || (arg & 63) == 2 ?
                    OperandRole{"character identity", "identity-class"} : OperandRole{"recruitment template", "recruitment-template"};
                break;
            case 0x17: if (reg == 0) return {"calendar day-count byte", {}}; break;
            case 0x19: if (reg == 0) return {"replacement world location", "world-location"}; break;
            case 0x1a: if (reg == 0) return {"horizontal world-map scroll", {}}; break;
            case 0x1b: if (reg == 7) return {"world-map update position", {}}; break;
            case 0x1c: case 0x1d: if (reg == 1) return {"report paragraph section byte", {}}; break;
            case 0x1e: if (reg == 0) return {"People report person", "report-person"}; break;
            case 0x22: if (reg == 0) return {"replacement identity/current classes", {}}; break;
            case 0x28: if (reg == 5) return {"portrait", {}}; break;
            case 0x2d: if (reg == 0) return {"Goth required", {}}; break;
            case 0x2e: if (reg == 0) return {"Goth amount byte", {}}; break;
            }
            return {};
        }
        return {};
    }
    QString preparation(int at, int reg, const QString &value, int literal = -1) const {
        if(reg==0 && at+3<bytes_.size() && instructionOffsets_.contains(at+2) &&
           byte(at+2)==0x16 && byte(at+3)>=5 && byte(at+3)<=8)
            return "Starting-squad selection uses the saved opening choices; this stored argument is unused";
        const auto role = consumerRole(at, reg);
        if (!role.label.isEmpty()) {
            const int op = byte(at);
            if (op >= 0x68 && op <= 0x6f && at + 3 < bytes_.size() &&
                instructionOffsets_.contains(at + 2) && byte(at + 2) == op + 0x20)
                return "Prepare " + role.label + " low byte · " + value;
            const auto name = literal >= 0 && !role.kind.isEmpty() && role.kind != "flag" ?
                reference(role.kind, literal, role.label) : QString{};
            if (role.kind == "flag" && literal >= 0) {
                const auto label = catalog_["flags"].toObject()[QString::number(literal)].toString();
                return label.isEmpty() ? "Prepare story flag " + value : "Prepare story flag · " + label;
            }
            return "Prepare " + role.label + " · " + (name.isEmpty() ? value : name);
        }
        if (original_) {
            const auto source=originalSteps_.value(at),lower=source.toLower();
            const auto label=annotationRole(at,reg);
            const bool classRole=label.contains("identity",Qt::CaseInsensitive) ||
                label.contains("current class",Qt::CaseInsensitive) ||
                lower.contains("story identity") || lower.contains("identity family") ||
                lower.contains("required current class") || lower.contains("excluded current class") ||
                lower.contains("unchanged secondary class");
            if(literal>=0 && classRole && !lower.contains("displacement")){
                const auto domain=lower.contains("current class") || lower.contains("secondary class")?
                    QString("required current class"):QString("required character identity");
                return "Prepare "+domain+" · "+reference("identity-class",literal&255,"class");
            }
            if(!label.isEmpty() && label.size()<=80)return "Prepare "+label+" · "+value;
            if(lower.contains("caller") && lower.contains("preserved through helper"))
                return "Retain caller input during the following identity check · "+value;
        }
        return QString("Prepare working value %1 · %2").arg(reg + 1).arg(value);
    }

    Description service(int op, int arg, const Values &r, int group) const {
        const bool write = (op >= 0x70 && op <= 0x77) || (op >= 0x80 && op <= 0x87);
        const bool alternate = op >= 0x78;
        const int d = op & 7, i = arg & 7, bank = arg >> 3;
        auto value = text(r[d], d);
        const auto idx = index(r[i], i);
        if (!alternate && arg >= 0xe4) {
            const auto definition = catalog_["properties"].toObject()[QString::number(arg)].toObject();
            if (write && definition["writeBehavior"].toString().contains("infinite loop"))
                return {"Unsupported write to " + property(arg, group) + " (game stalls)", {}};
            if (!write && arg == 0xf7) return {"Keep the current working value", "This native property read leaves its destination unchanged."};
            if (write && definition["storage"].toObject()["bytes"].toInt() == 1) value = low(r[d], d);
            return {write ? "Set " + property(arg, group) + " to " + value : "Read " + property(arg, group), {}};
        }
        if (alternate && arg >= 0xe5) {
            if (write) return {"Leave game state unchanged", "This alternate property write has no effect."};
            if (arg < 0xfd) return {"Keep the current working value", "This alternate property read leaves its destination unchanged."};
            return {arg == 0xfd ? "Read game-clock hour" : arg == 0xfe ? "Read game-clock minute" : "Read game-clock second", {}};
        }
        if (!alternate && arg < 8) {
            if (write) return {"Unsupported working-value write (game stalls)", "The native interpreter enters a self-loop for this instruction."};
            return {"Copy " + low(r[i], i), {}};
        }
        const auto bankName = alternate ? (write ? "altSetters" : "altGetters") : (write ? "setters" : "getters");
        const auto definitions = catalog_["banks"].toObject()[bankName].toArray();
        const auto definition = bank < definitions.size() ? definitions[bank].toObject() : QJsonObject{};
        const auto meaning = definition["meaning"].toString();
        if (meaning.contains("null function pointer") || definition["name"].toString().contains("invalid"))
            return {write ? "Invalid game-data write (no native handler)" : "Invalid game-data read (no native handler)", {}};
        if (definition["status"] != "verified" && !(bank >= 19 && bank <= 21 && !alternate))
            return {QString("Unresolved %1 game-data %2 · service %3").arg(alternate ? "alternate" : "primary", write ? "write" : "read").arg(bank), meaning};
        if (write && definition["storage"].toObject()["type"] == "u8") value = low(r[d], d);
        if (!alternate && bank == 1) {
            const auto member = QString("member position %1 of squad %2").arg(idx, index(r[7], 7));
            return {write ? "Set " + member + " to character " + value : "Read " + member, {}};
        }
        if (!alternate && bank == 12) {
            QString flag;
            const int token = r[i].literal < 0 ? 0 : qint16(r[i].literal);
            if (r[i].literal >= 0) {
                const int id = write ? token : qAbs(token);
                const auto rowNames = original_ ? catalog_["rows"].toObject()[QString::number(row_)].toObject()["flags"].toObject() : QJsonObject{};
                const auto label = rowNames[QString::number(id)].toString(catalog_["flags"].toObject()[QString::number(id)].toString());
                flag = label.isEmpty() ? QString("Flag %1").arg(id) : QString("Flag %1 (%2)").arg(id).arg(label);
            } else flag = "story flag " + idx;
            if (!write) return {(r[i].literal >= 0 && token < 0 ? "Read inverse of " : "Read ") + flag, {}};
            if (r[d].literal >= 0) return {(r[d].literal == 1 ? "Set " : "Clear ") + flag, {}};
            return {QString("Set %1 if %2 = 1; otherwise clear it").arg(flag, value), {}};
        }
        if (!alternate && bank >= 19 && bank <= 21) {
            const auto town = write ? value : idx;
            if (write) return {QString("%1 town %2 flags %3").arg(bank == 21 ? "Clear" : "Set", town, bank == 19 ? "mask 2" : bank == 20 ? "mask 4" : "mask 6"),
                               "The supplied value selects the town; the index argument is ignored."};
            return {QString("Read town %1 flags %2").arg(town, bank == 19 ? "mask 2" : bank == 20 ? "mask 4" : "mask 6 clear test"), {}};
        }
        if (!alternate && bank == 26 && !write) return {"Prepare zero", {}};
        if (!alternate && bank == 22 && write)
            return {"Write world-location state · " + typed(r[i], i, "world-location", "ID") + " = " + low(r[d], d), {}};
        if (!alternate && bank == 23)
            return {write ? "Write world road " + typed(r[i], i, "world-road", "ID") + " = " + low(r[d], d) :
                            "Read world road " + typed(r[i], i, "world-road", "ID"), {}};
        if (!alternate && (bank == 24 || bank == 25)) {
            const auto subject = QString("scene actor %1 %2 class").arg(idx, bank == 24 ? "identity" : "current");
            return {write ? "Set " + subject + " to " + typed(r[d], d, "identity-class", "class") : "Read " + subject, {}};
        }
        if (!alternate && bank == 26 && write && r[i].literal == 255)
            return {"Set reward-panel item to " + typed(r[d], d, "reward-display", "reward token"), {}};
        if (!alternate && bank == 27) {
            const auto name = typed(r[i], i, "transient-game-state", "temporary gameplay flag");
            if (!write) return {"Read " + name, {}};
            return {r[d].literal >= 0 ? (r[d].literal ? "Set " : "Clear ") + name : "Set " + name + " when " + value + " is nonzero; otherwise clear it", {}};
        }
        if (alternate && !write && (bank == 1 || bank == 5))
            return {"Find inventory slot for " + typed(r[i], i, bank == 1 ? "consumable" : "equipment", bank == 1 ? "consumable" : "equipment"), "Missing items return 511; this does not add an item."};
        if (alternate && write && bank == 9) {
            const auto side = r[d].literal == 0 ? QString("army") : r[d].literal == 1 ? QString("opposing roster") : "side " + value + " (0 army, 1 opposing)";
            return {"Permanently remove character " + idx + " from " + side, "Clears the character record and compacts its side's active character list."};
        }
        if (alternate && write && bank == 10)
            return {"Write mission marker for " + typed(r[i], i, "world-location", "world location") + " = " + low(r[d], d), {}};
        const auto name = definition["name"].toString(QString("service %1").arg(bank));
        return {write ? "Set " + name + " at " + idx + " to " + value : "Read " + name + " at " + idx, {}};
    }

    Description instruction(int at, const Values &r, int group) const {
        const int op = byte(at), arg = byte(at + 1), d = op & 7;
        if ((op >= 0x60 && op <= 0x67) || (op >= 0x70 && op <= 0x87)) return service(op, arg, r, group);
        if (op >= 0x68 && op <= 0x6f) return {preparation(at, d, QString::number(arg), arg), {}};
        if (op >= 0x88 && op <= 0x8f) {
            const auto value = r[d].literal >= 0 ? QString::number((r[d].literal & 255) | (arg << 8)) :
                QString("%1 + %2").arg(low(r[d], d)).arg(arg << 8);
            return {preparation(at, d, value, r[d].literal >= 0 ? (r[d].literal & 255) | (arg << 8) : -1), {}};
        }
        if (op >= 0x90 && op <= 0x9f) {
            const bool immediate = op >= 0x98;
            const int operation = arg >> (immediate ? 5 : 3);
            if (operation > 6) return {"Leave working value unchanged", "This native calculation selector has no effect."};
            const auto rhs = immediate ? QString::number(arg & 31) : text(r[arg & 7], arg & 7);
            if ((operation == 5 || operation == 6) && (immediate ? (arg & 31) == 0 : r[arg & 7].literal == 0))
                return {"Invalid calculation: division by zero", "The native interpreter raises a break instruction."};
            const QStringList operations{"bitwise AND", "bitwise OR", "+", "−", "×", "÷", "remainder"};
            if(immediate && operation==2 && (arg&31)==1){
                const auto role=r[d].text.toLower();
                if(role.contains("transient gameplay flag"))return {"Advance to the next transient gameplay flag",{}};
                if(role.contains("candidate") && role.contains("inventory slot"))return {"Advance to the next "+role.section("candidate ",1),{}};
            }
            return {QString("Calculate %1 %2 %3").arg(text(r[d], d), operations[operation], rhs), "The result is truncated to 16 bits; division and remainder use unsigned values."};
        }
        if (comparison(op)) {
            const bool single = (op & 15) >= 12, signedTest = (op >> 4) >= 12;
            const int a = single ? op & 3 : (op >> 2) & 3, b = op & 3;
            const auto operand = [&](int reg) { return signedTest && r[reg].literal >= 0 ? QString::number(qint16(r[reg].literal)) : text(r[reg], reg); };
            return {QString("Check %1 %2 %3").arg(operand(a), relation(op), single ? "0" : operand(b)),
                    QString("If true, continue at step %1; otherwise step %2.%3").arg(at + 2 + qint8(arg) * 2).arg(at + 2)
                        .arg(signedTest ? " This comparison treats 16-bit values as signed." : "")};
        }
        const auto r0 = text(r[0], 0);
        switch (op) {
        case 1: return {QString("Continue at step %1").arg(at + 2 + qint8(arg) * 2), {}};
        case 2: case 3: {
            const auto target = r[0].literal >= 0 ? QString::number(quint16((at + 2) / 2 + (arg << 8) + (r[0].literal & 255)) * 2) :
                QString("the relative destination using high byte %1 and %2").arg(arg).arg(low(r[0], 0));
            return {(op == 3 ? "Run shared event logic at " : "Continue at ") + target,
                    op == 3 ? "Return resumes at the step after this call; calls and saved values share one native stack." : QString{}};
        }
        case 4: return {"Return to the calling event logic", {}};
        case 5: return {"Preserve " + text(r[arg & 7], arg & 7) + " for later", "Saved working values share the native call/return stack."};
        case 6: {
            const auto role=consumerRole(at,arg&7);
            return {role.label.isEmpty()?QString("Restore saved caller context"):"Restore "+role.label,
                    QString("Restores the native stack's top value into working value %1.").arg((arg & 7) + 1)};
        }
        case 0x10: return {"Play cutscene · " + typed(r[0], 0, "scene", "cutscene"),
                          QString("Launch mode %1; death subject uses the %2 side.%3").arg(arg & 3).arg(arg & 128 ? "player" : "opposing")
                              .arg(arg & 64 ? " Clear death-context request." : "")};
        case 0x11: {
            if (r[0].literal >= 0) {
                const int token = (arg << 8) | (r[0].literal & 255);
                return {token == 1 ? "Finish this event" : "Request · " + request(token & 255),
                        token == 1 ? QString{} : QString("Exit through the requested game-mode transition (request %1).").arg(token & 255)};
            }
            return {"Finish or request the next game operation", "The request uses " + low(r[0], 0) + QString(" with high byte %1; exact token 1 finishes this event.").arg(arg)};
        }
        case 0x12: return {"Load event program · " + reference("event-row", arg, "row"),
                          r[0].literal >= 0 ? QString("Clear flags %1 through %2 (exclusive) before changing programs.").arg(r[0].literal & 255).arg((r[0].literal >> 8) & 255) :
                              "Clear the flag range given by the low/high bytes of " + r0 + " before changing programs."};
        case 0x13: return {request(arg), "Resume at the next event step after the requested operation is accepted."};
        case 0x15: return {"Record history · " + reference("scene-history", arg, "entry"),
                          QString("Date: year %1, month %2, day %3. Duplicate history entries are not added.").arg(r0, text(r[1], 1), text(r[2], 2))};
        case 0x16: {
            if (arg >= 5 && arg <= 8) return {QString("Queue starting squad %1 from the saved opening choices").arg(arg - 4), {}};
            if (arg & 128) return {"Remove army characters matching · " + typed(r[0], 0, "identity-class", "identity"), "Permanently removes matching identities and clears the pending army operation."};
            const int mode = arg & 63;
            const auto title = mode == 2 ? "Queue import of opposing squad containing · " + typed(r[0], 0, "identity-class", "identity") :
                (mode == 0 ? "Queue recruitment of squad · " : "Queue recruitment of characters · ") + typed(r[0], 0, "recruitment-template", "template");
            return {title, QString("Processed by the next queued-army operation request.%1").arg(arg & 64 && mode != 2 ? " Apply oath-based stat adjustments." : "")};
        }
        case 0x17: case 0x2e: {
            if (r[0].literal >= 0) {
                const int amount = qint16((arg << 8) | (r[0].literal & 255));
                if (op == 0x17) return {QString("%1 the calendar by %2 days").arg(amount < 0 ? "Rewind" : "Advance").arg(qAbs(amount)), {}};
                return {QString("%1 %2 Goth").arg(amount < 0 ? "Give the army" : "Take from the army").arg(qAbs(amount)), {}};
            }
            return {op == 0x17 ? "Adjust the calendar by a calculated day count" : "Change army Goth by a calculated amount",
                    QString("Signed 16-bit amount uses high byte %1 and %2.%3").arg(arg).arg(low(r[0], 0))
                        .arg(op == 0x2e ? " Positive amounts are subtracted; negative amounts are added." : "")};
        }
        case 0x18: return {arg ? "Request world-map travel to · " + reference("world-location", arg, "location") : "Clear forced world-map destination", "The world map processes the request using available roads."};
        case 0x19: return {"Substitute world-location snapshots of " + reference("world-location", arg, "location") + " with " + typed(r[0], 0, "world-location", "location", true),
                          "Changes subsequent location snapshots; it does not load a mission."};
        case 0x1a: return {QString("Save world-map scroll · X %1, Y %2").arg(r0).arg(arg), {}};
        case 0x1b: {
            // These contextual stream roles depend on all preceding location,
            // action and terminator bytes, not merely this word's operand.
            if (original_ && originalSteps_.contains(at)) {
                const auto source = originalSteps_[at];
                const int colon = source.indexOf(": ");
                if (colon >= 0) return {"Update world map · " + source.mid(colon + 2), {}};
                return {"Update world map · " + source, {}};
            }
            return {arg == 255 ? "Append world-map update chain/list terminator" : QString("Append world-map location/action update byte %1").arg(arg),
                    "Writes at update position " + text(r[7], 7) + "; advances that position and installs the update stream. Location/action meaning depends on the surrounding stream."};
        }
        case 0x1c: case 0x1d: {
            if (original_ && originalSteps_.contains(at)) return {originalSteps_[at].section(';', 0, 0), {}};
            const auto id = r[1].literal >= 0 ? QString::number((arg << 8) | (r[1].literal & 255)) :
                QString("high byte %1 + %2").arg(arg).arg(low(r[1], 1));
            return {(op == 0x1c ? "Include report paragraph section " : "Exclude report paragraph section ") + id,
                    "The same section bit affects the applicable Tips/Miscellany context; the prepared first argument is ignored."};
        }
        case 0x1e: return {arg ? "Select biography variant " + QString::number(arg) + " for " + typed(r[0], 0, "report-person", "report person", true) :
                                   "Hide from the People board · " + typed(r[0], 0, "report-person", "report person", true), {}};
        case 0x1f: return {"Select People relationships · " + reference("relationship-board", arg, "layout", "report:people"), {}};
        case 0x20: return {"Initialize the first Legion movement group", "Uses the native defaults and its original repeated scan of squad 0."};
        case 0x21: return {arg ? "Enable the Legion-count Army menu entry" : "Disable the Legion-count Army menu entry and clear all Legion groups", {}};
        case 0x22: {
            const auto replacement = r[0].literal >= 0 ? QString("identity %1; current class %2").arg(reference("identity-class", (r[0].literal >> 8) & 255, "class"), reference("identity-class", r[0].literal & 255, "class")) :
                "identity from the high byte and current class from the low byte of " + r0;
            return {"Replace classes for " + reference("identity-class", arg, "identity") + " · " + replacement,
                    "Skips the native excluded current classes 36, 37, 45, 46, 47 and 48, then refreshes matching characters."};
        }
        case 0x28: return {"Resolve portrait directory for " + low(r[5], 5), "Produces the portrait bank and variant in working values 7 and 8."};
        case 0x29: return {"Snapshot the current world location", "Uses its low six bits, applying any configured location substitution."};
        case 0x2d: return {"Check whether army Goth is less than " + r0,
                          QString("If true, continue at step %1; otherwise step %2. The comparison is unsigned.").arg(at + 2 + qint8(arg) * 2).arg(at + 2)};
        case 0x2f: return {"Finish this event", "The event scheduler may continue to another handler."};
        default:
            if (catalog_["opcodes"].toObject()[QString::number(op)].toString() == "native no-op")
                return {"Leave game state unchanged", "This opcode is a native no-op; its stored operand is ignored."};
            return {QString("Unknown game instruction %1").arg(op), {}};
        }
    }
};
} // namespace lordly
