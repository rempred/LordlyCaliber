#include "engine/event_script.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>

namespace lordly {
namespace {
const QJsonObject &catalog() {
    static const auto value = [] {
        QFile file(":/game/director/event-semantics.json");
        if (!file.open(QIODevice::ReadOnly)) throw Error("Missing event descriptions");
        const auto object = QJsonDocument::fromJson(file.readAll()).object();
        if (object["format"] != "lordly-event-semantics" || object["version"] != 1 ||
            object["rows"].toObject().size() != 113) throw Error("Invalid event descriptions");
        return object;
    }();
    return value;
}
QJsonObject rowInfo(int row) { return catalog()["rows"].toObject()[QString::number(row)].toObject(); }
QString address(int at) { return QString("@%1").arg(at, 4, 16, QChar('0')).toUpper(); }
QString namedFlag(int flag, const QString &name) {
    return name.isEmpty() ? QString("Flag %1").arg(flag) : QString("Flag %1 (%2)").arg(flag).arg(name);
}
const QMap<int, QJsonArray> &instructions(int row) {
    static const auto rows = [] {
        QMap<int, QMap<int, QJsonArray>> result;
        const auto source = catalog()["rows"].toObject();
        for (auto r = source.begin(); r != source.end(); ++r)
            for (const auto &value : r.value().toObject()["steps"].toArray()) {
                const auto step = value.toArray(); result[r.key().toInt()][step[0].toInt()] = step;
            }
        return result;
    }();
    static const QMap<int, QJsonArray> empty;
    const auto found = rows.constFind(row); return found == rows.cend() ? empty : found.value();
}
bool comparison(int op) {
    return op >= 0xa0 && op <= 0xff &&
        QSet<int>{1,2,3,6,7,11,12,13,14,15}.contains(op & 15);
}
bool sameInstruction(int current,int original) {
    return current == original || (comparison(current) && comparison(original) &&
        (current & 15) == (original & 15));
}
bool shortBranch(int op) { return op == 1 || op == 0x2d || comparison(op); }
QString bankFor(int op) {
    if (op >= 0x60 && op <= 0x67) return "getters";
    if (op >= 0x70 && op <= 0x77) return "setters";
    if (op >= 0x78 && op <= 0x7f) return "altGetters";
    if (op >= 0x80 && op <= 0x87) return "altSetters";
    return {};
}
QMap<int, QString> services(int op) {
    const auto bank = bankFor(op); QMap<int, QString> result;
    if (bank.isEmpty()) return result;
    const bool alternate = bank.startsWith("alt"), setter = bank.endsWith("Setters") || bank == "setters";
    for (const auto &value : catalog()["banks"].toObject()[bank].toArray()) {
        const auto entry = value.toObject(); const int id = entry["id"].toInt();
        const auto name = entry["name"].toString();
        if (name.contains("invalid") || name.contains("overlapping") || (setter && !id)) continue;
        for (int r = 0; r < 8 && id*8+r < 0xe4; ++r)
            result[id*8+r] = QString("%1 · R%2").arg(name).arg(r);
    }
    if (!alternate) {
        const auto props = catalog()["properties"].toObject();
        for (auto p = props.begin(); p != props.end(); ++p) {
            const auto entry = p.value().toObject();
            if (!setter || !entry["writeBehavior"].toString().contains("infinite loop"))
                result[p.key().toInt()] = entry["name"].toString();
        }
    } else if (!setter) { result[0xfd]="Clock hour"; result[0xfe]="Clock minute"; result[0xff]="Clock second"; }
    return result;
}
QMap<int, QString> referenceNames(const QString &kind, const QString &scope) {
    static const auto names = [] {
        QMap<QString, QMap<int, QString>> result;
        for (const auto &v : catalog()["names"].toArray()) {
            const auto n = v.toArray(); result[n[0].toString()+"/"+n[1].toString()][n[2].toInt()] = n[3].toString();
        }
        return result;
    }();
    auto result = names.value(kind+"/global");
    const auto local = names.value(kind+"/"+scope);
    for (auto it=local.begin();it!=local.end();++it) result[it.key()]=it.value();
    return result;
}
}
QSet<int> EventScript::instructionOffsets(int row) {
    const auto &source = instructions(row); QSet<int> result;
    for (auto it=source.begin();it!=source.end();++it) result.insert(it.key());
    return result;
}
QVector<EventParameter> EventScript::parameters(const QByteArray &bytes, int row, int at) {
    const auto &source = instructions(row);
    if (!source.contains(at) || at+1 >= bytes.size()) throw Error("This location is not an event instruction");
    const int op=u8(bytes,at), arg=u8(bytes,at+1);
    QVector<EventParameter> fields;
    if(catalog()["opcodes"].toObject()[QString::number(op)]=="native no-op"||
       op==4||op==0x20||op==0x29||op==0x2f)return fields;
    auto add=[&](QString key,QString label,int value,int min,int max,QVector<EventParameter::Part> parts) -> EventParameter & {
        fields << EventParameter{key,label,value,min,max,{},parts}; return fields.last();
    };
    if (shortBranch(op)) {
        auto &f=add("destination","Destination",at+2+qint8(arg)*2,0,bytes.size()-2,{{at}});
        for (auto it=source.begin();it!=source.end();++it)
            if (it.key()>=at+2-256 && it.key()<=at+2+254) f.choices[it.key()]=address(it.key());
        if(comparison(op)) {
            auto &relation=add("comparison","Comparison",op>>4,10,15,{});
            relation.choices={{10,"="},{11,"≠"},{12,">"},{13,"<"},{14,"≥"},{15,"≤"}};
        }
    } else if ((op==2 || op==3) && at>=2 && source.contains(at-2) && u8(bytes,at-2)==0x68) {
        const int target=quint16((at+2)/2 + (arg<<8) + u8(bytes,at-1))*2;
        auto &f=add("destination",op==3?"Call destination":"Destination",target,0,bytes.size()-2,{{at-2},{at,255,0,8}});
        for (auto it=source.begin();it!=source.end();++it) f.choices[it.key()]=address(it.key());
    } else if (op>=0x68 && op<=0x6f) {
        const bool wide=source.contains(at+2) && u8(bytes,at+2)==op+0x20;
        auto &f=add("value",QString("R%1 value").arg(op&7),arg | (wide ? u8(bytes,at+3)<<8:0),0,wide?65535:255,{{at}});
        if (wide) f.parts << EventParameter::Part{at+2,255,0,8};
        // Roles are only retained while the program's command bytes and service
        // operands are unchanged. Changing a consumer cannot retain a stale ID kind.
        bool sameConsumers=true;
        for (auto it=source.begin();it!=source.end();++it) {
            if(it.key()+1>=bytes.size()) {sameConsumers=false;break;}
            const int original=it.value()[3].toInt(), code=original>>8;
            const int currentOp=u8(bytes,it.key());
            if (!sameInstruction(currentOp,code) || (!((code>=0x68&&code<=0x6f)||(code>=0x88&&code<=0x8f)||shortBranch(code)) && u16(bytes,it.key())!=original)) {sameConsumers=false;break;}
        }
        if(sameConsumers) {
            const auto ref=source[at][4].toObject(); const auto kind=ref["kind"].toString();
            if(!kind.isEmpty()) {
                f.kind=kind;
                auto label=kind;label.replace('-',' ');f.label=label+QString(" · R%1").arg(op&7);
                f.choices=referenceNames(kind,ref["scope"].toString());
                if(kind=="flag") {f.maximum=qMin(f.maximum,255);for(int n=0;n<256;++n)f.choices[n]=flagName(n,row);}
                for(auto it=f.choices.begin();it!=f.choices.end();)if(it.key()<f.minimum||it.key()>f.maximum)it=f.choices.erase(it);else ++it;
            }
        }
    } else if (op>=0x88 && op<=0x8f) {
        add("value",QString("R%1 upper byte").arg(op&7),arg,0,255,{{at}});
    } else if (!bankFor(op).isEmpty()) {
        auto &f=add("service","Value to read / write",arg,0,255,{{at}});f.choices=services(op);
        if(!f.choices.contains(arg))f.choices[arg]=QString("Existing service ID %1").arg(arg);
    } else if (op>=0x90 && op<=0x9f) {
        const bool immediate=op>=0x98;const int shift=immediate?5:3,mask=(1<<shift)-1;
        auto &f=add("operation","Calculation",arg>>shift,0,6,{{at,255^mask,shift}});
        f.choices={{0,"AND"},{1,"OR"},{2,"Add"},{3,"Subtract"},{4,"Multiply"},{5,"Divide"},{6,"Remainder"}};
        auto &v=add("argument",immediate?"Amount":"Source register",arg&mask,0,mask,{{at,mask}});
        if(!immediate)for(int r=0;r<8;++r)v.choices[r]=QString("R%1").arg(r);
    } else if (op==5 || op==6) {
        auto &f=add("register","Register",arg&7,0,7,{{at,7}});
        for(int r=0;r<8;++r)f.choices[r]=QString("R%1").arg(r);
    } else if (op==0x10) {
        auto &mode=add("mode","Request mode",arg&3,0,3,{{at,3}});
        mode.choices={{0,"Normal (0)"},{1,"Request flag 0x2000 (1)"},{2,"Request flag 0x4000 (2)"},{3,"Disable death interception (3)"}};
        auto &side=add("deathSide","Death subject side",(arg>>7)&1,0,1,{{at,128,7}});
        side.choices={{0,"Opposing army"},{1,"Player army"}};
        auto &context=add("deathContext","Death context",(arg>>6)&1,0,1,{{at,64,6}});
        context.choices={{0,"Keep context"},{1,"Clear context"}};
    } else if ((op==0x17||op==0x2e) && at>=2 && source.contains(at-2) && u8(bytes,at-2)==0x68) {
        add("signedAmount",op==0x17?"Days to advance":"Goth change",qint16((arg<<8)|u8(bytes,at-1)),-32768,32767,{{at-2},{at,255,0,8}});
    } else {
        const auto name=catalog()["opcodes"].toObject()[QString::number(op)].toString();
        auto &f=add("operand",name=="native no-op"||op==4||op==0x2f?"Unused stored operand":"Operand",arg,0,255,{{at}});
        if(op==0x12){f.label="Event row";f.maximum=114;for(int r=0;r<115;++r)f.choices[r]=rowName(r);}
        if(op==0x16){f.label="Army action";f.choices={{0,"Queue squad recruitment"},{1,"Queue individual recruitment"},{2,"Queue recruitment of opposing squad"},{5,"Initial army selection 5"},{6,"Initial army selection 6"},{7,"Initial army selection 7"},{8,"Initial army selection 8"},{64,"Squad recruitment · initialization option 1"},{65,"Individual recruitment · initialization option 1"},{66,"Opposing squad recruitment · initialization option 1"},{129,"Permanently remove identity in R0"}};if(!f.choices.contains(arg))f.choices[arg]=QString("Action ID %1").arg(arg);}
        if(op==0x15)f.label="History entry";
        if(op==0x19){f.label="Map location";f.choices=referenceNames("world-location","global");}
        if(op==0x1a)f.label="World map vertical scroll";
        if(op==0x1e)f.label="Biography variant";
        if(op==0x1f)f.label="People relationship layout";
        if(op==0x22){f.label="Identity class to replace";f.kind="identity-class";f.choices=referenceNames(f.kind,"global");}
        if(op==0x10)f.label="Cutscene request flags";
    }
    return fields;
}
QJsonObject EventScript::parameterEdit(const QByteArray &bytes,int row,int at,const QString &key,int value) {
    for(const auto &field:parameters(bytes,row,at))if(field.key==key) {
        if(value==field.value)return {};
        const bool extensible=field.kind=="identity-class"||field.kind=="equipment"||field.kind=="consumable";
        if(value<field.minimum||value>field.maximum||(!extensible&&!field.choices.isEmpty()&&!field.choices.contains(value)))
            throw Error("Choose a supported value for this instruction");
        if(key=="comparison") {
            const QJsonObject edits{{QString("comparison:%1").arg(at),value}};
            applyOperands(bytes,row,edits);return edits;
        }
        int encoded=value;
        if(key=="destination")encoded=(value-(at+2))/2;
        QJsonObject edits;
        for(const auto &part:field.parts) {
            const int byte=(u8(bytes,part.offset+1)&~part.mask)|(((quint16(encoded)>>part.valueShift)<<part.shift)&part.mask);
            edits[QString::number(part.offset)]=byte;
        }
        applyOperands(bytes,row,edits);return edits;
    }
    throw Error("This instruction has no such parameter");
}
QByteArray EventScript::applyOperands(const QByteArray &bytes,int row,const QJsonObject &edits) {
    if(edits.isEmpty())return bytes;
    const auto &source=instructions(row);auto result=bytes;QSet<int> changed;
    for(auto it=edits.begin();it!=edits.end();++it) {
        const bool relation=it.key().startsWith("comparison:");
        const auto offsetKey=relation?it.key().mid(11):it.key();
        bool ok=false;const int at=offsetKey.toInt(&ok),value=it.value().toInt(-1);
        if(!ok||QString::number(at)!=offsetKey||!source.contains(at)||at+1>=bytes.size()||
           !it.value().isDouble()||it.value().toDouble()!=value||value<0||value>255)
            throw Error("Invalid event operand edit");
        const int op=u8(bytes,at);
        if(!sameInstruction(op,source[at][3].toInt()>>8))
            throw Error("Event operand no longer belongs to its original command");
        if(relation) {
            if(!comparison(op)||value<10||value>15)
                throw Error("Choose a supported comparison for a conditional instruction");
            put8(result,at,quint8((value<<4)|(op&15)));changed.insert(at);continue;
        }
        if(value==u8(bytes,at+1))continue;
        const auto validServices=services(op);
        if(!bankFor(op).isEmpty()&&!validServices.contains(value))throw Error("Unsupported native event service");
        if(op==0x12&&value>=115)throw Error("Event row is outside the event directory");
        if(op>=0x98&&op<=0x9f&&(value>>5==5||value>>5==6)&&!(value&31))throw Error("Cannot divide by zero");
        put8(result,at+1,quint8(value));changed.insert(at);
    }
    for(auto it=source.begin();it!=source.end();++it) {
        const int at=it.key();if(at+1>=result.size())throw Error("Truncated event program");
        const int op=u8(result,at),arg=u8(result,at+1);
        if(shortBranch(op)&&changed.contains(at)&&!source.contains(at+2+qint8(arg)*2))throw Error("Branch destination must be an instruction");
        if((op==2||op==3)&&at>=2&&u8(result,at-2)==0x68&&(changed.contains(at)||changed.contains(at-2))) {
            const int target=quint16((at+2)/2+(arg<<8)+u8(result,at-1))*2;
            if(!source.contains(target))throw Error("Call or jump destination must be an instruction");
        }
    }
    return result;
}
QString EventScript::rowName(int row) {
    const auto name = rowInfo(row)["title"].toString();
    return name.isEmpty() ? QString("Event row %1").arg(row) : QString("%1 · %2").arg(row).arg(name);
}
QString EventScript::flagName(int flag, int row, int offset) {
    const auto key = QString::number(flag);
    if(offset>=0){const auto step=instructions(row).value(offset);const auto scoped=step.size()>2?step[2].toObject()[key].toString():QString{};if(!scoped.isEmpty())return namedFlag(flag,scoped);}
    const auto name = rowInfo(row)["flags"].toObject()[key].toString(
        catalog()["flags"].toObject()[key].toString());
    return namedFlag(flag, name);
}
QVector<EventHandler> EventScript::display(const QByteArray &bytes, int row, bool originalSceneContext) {
    const auto info = rowInfo(row);
    const bool original = originalSceneContext && !info.isEmpty() &&
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex() == info["sha256"].toString().toLatin1();
    QVector<EventHandler> decoded;
    try { decoded = decode(bytes, row); }
    catch (const Error &) {
        if (info.isEmpty() || bytes.size() < info["byteLength"].toInt()) throw;
        // Special program layouts can still be inspected after same-size code
        // edits, but only if every non-instruction word retains its layout.
        const auto structure = info["structure"].toArray();
        for (int n = 0; n < structure.size(); n += 2) {
            const int at=structure[n].toInt(),value=u16(bytes,at);
            if (value != structure[n+1].toInt() && !(value>=info["byteLength"].toInt()&&value<bytes.size()&&value%2==0)) throw;
        }
    }

    QMap<int, EventStep> controls;
    for (const auto &handler : decoded)
        for (const auto &step : handler.steps) controls.insert(step.offset, step);
    const auto descriptions = info["steps"].toArray();
    if (descriptions.isEmpty()) return decoded;
    const auto handlers = info["handlers"].toArray();
    QVector<EventHandler> result;
    int handlerIndex = -1, until = -1;
    for (int n = 0; n < descriptions.size(); ++n) {
        const auto description = descriptions[n].toArray();
        const int at = description[0].toInt();
        if (at < until) continue;
        const int oldIndex = handlerIndex;
        while (handlerIndex + 1 < handlers.size() && handlers[handlerIndex+1].toObject()["entry"].toInt() <= at)
            ++handlerIndex;
        if (result.isEmpty() || oldIndex != handlerIndex) {
            const auto h = handlerIndex < 0 ? QJsonObject{} : handlers[handlerIndex].toObject();
            EventHandler next; next.entry = h["entry"].toInt(at); next.group = h["group"].toInt(-1);
            // A changed program cannot retain its original handler's story purpose.
            next.title = original ? h["title"].toString("Program instructions") : "Current program instructions";
            result << next;
        }
        EventStep step; step.offset = at; step.researched = original;
        if (original) {
            const auto control = controls.value(at);
            const int nextEntry = handlerIndex+1 < handlers.size() ? handlers[handlerIndex+1].toObject()["entry"].toInt() : bytes.size();
            const bool compound = control.flag >= 0 && control.length == 6 && at+6 <= nextEntry &&
                n+2 < descriptions.size() && descriptions[n+1].toArray()[0].toInt() == at+2 &&
                descriptions[n+2].toArray()[0].toInt() == at+4;
            if (compound) {
                step = control; step.researched = true;
                QString name;
                for (int word = 0; word < 3; ++word) {
                    const auto candidate = descriptions[n+word].toArray()[2].toObject()[QString::number(step.flag)].toString();
                    if (!candidate.isEmpty()) name = candidate;
                }
                step.flagLabel = name.isEmpty() ? flagName(step.flag, row) : namedFlag(step.flag, name);
                step.text = step.check ? QString("CHECK %1 = %2 → %3; otherwise → %4")
                    .arg(step.flagLabel, step.value ? "ON" : "OFF", address(step.target), address(at+6))
                    : QString("%1 %2 = %3").arg(step.value ? "SET" : "CLEAR", step.flagLabel, step.value ? "ON" : "OFF");
            } else {
                step.text = description[1].toString();
                if (control.length == 2) step.target = control.target;
            }
        } else {
            const int op = u8(bytes, at), arg = u8(bytes, at+1);
            const auto name = catalog()["opcodes"].toObject()[QString::number(op)].toString("Unknown command");
            step.text = QString("%1 · opcode %2, operand %3 (0x%4)").arg(name)
                .arg(op, 2, 16, QChar('0')).arg(arg).arg(arg, 2, 16, QChar('0'));
            if(controls.contains(at))step=controls[at];
            else if(op>=0x68&&op<=0x6f) {
                const auto ref=description[4].toObject();
                step.text=QString("Prepare R%1 = %2").arg(op&7).arg(arg);
                if(!ref.isEmpty()) {
                    const int full=(at+3<bytes.size()&&u8(bytes,at+2)==op+0x20)?arg|(u8(bytes,at+3)<<8):arg;
                    const auto label=referenceNames(ref["kind"].toString(),ref["scope"].toString()).value(full);
                    if(!label.isEmpty())step.text+=QString(" (%1 %2: %3)").arg(ref["kind"].toString()).arg(full).arg(label);
                }
            }
            if(shortBranch(op))step.target=at+2+qint8(arg)*2;
            if(!bankFor(op).isEmpty()) {
                const auto service=services(op).value(arg,QString("service ID %1").arg(arg));
                step.text=bankFor(op).contains("etters")&&!bankFor(op).contains("Setters")&&bankFor(op)!="setters"
                    ? QString("Read %1 → R%2").arg(service).arg(op&7)
                    : QString("Write R%1 → %2").arg(op&7).arg(service);
            }
            if(op>=0x90&&op<=0x9f) {
                const bool immediate=op>=0x98;const int shift=immediate?5:3;
                const QStringList operators{"AND","OR","+","−","×","÷","remainder"};
                const int operation=arg>>shift;
                step.text=operation<operators.size()?QString("R%1 = R%1 %2 %3").arg(op&7)
                    .arg(operators[operation],immediate?QString::number(arg&31):QString("R%1").arg(arg&7)):
                    QString("Calculation ID %1 · R%2 (native no-op)").arg(operation).arg(op&7);
            }
        }
        result.last().steps << step;
        until = at + step.length;
    }
    // Appended handlers belong to authored trees or other recognized extensions.
    // Keep their current instructions visible rather than losing them behind
    // the fixed retail annotation inventory.
    QSet<int> presented;for(const auto &h:result)for(const auto &s:h.steps)for(int n=0;n<s.length;n+=2)presented.insert(s.offset+n);
    for(const auto &h:decoded){EventHandler extra=h;extra.steps.clear();extra.title="Added event instructions";
        for(const auto &s:h.steps)if(!presented.contains(s.offset)){extra.steps<<s;for(int n=0;n<s.length;n+=2)presented.insert(s.offset+n);}
        if(!extra.steps.isEmpty())result<<extra;
    }
    return result;
}
} // namespace lordly
