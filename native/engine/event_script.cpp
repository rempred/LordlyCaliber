#include "engine/event_script.h"
#include "engine/retail_event_gate.h"
#include <QMap>
#include <algorithm>
#include <array>
namespace lordly {
QString EventScript::groupName(int group) {
    return group == 0 ? "Mission entry" : group == 5 ? "Mission completion"
                                                    : QString("Event group %1").arg(group);
}
namespace {
QString address(int at) { return QString("@%1").arg(at, 4, 16, QChar('0')).toUpper(); }
bool comparison(int op) {
    return op >= 0xa0 && op <= 0xff &&
        QSet<int>{1,2,3,6,7,11,12,13,14,15}.contains(op & 15);
}
QVector<int> successors(const QByteArray &bytes, int at, int appendedFrom = 0x7fffffff) {
    const int op = u8(bytes, at), arg = u8(bytes, at + 1), next = at + 2;
    if(op==8)if(auto target=RetailEventGate::destination(bytes,at))return {*target};
    if ((op==2||op==3)&&at>=appendedFrom&&at>=2&&u8(bytes,at-2)==0x68) {
        const int target=quint16(next/2+(arg<<8)+u8(bytes,at-1))*2;
        return op==2?QVector<int>{target}:QVector<int>{next,target};
    }
    if (op == 0x11 || op == 0x12 || op == 0x2f || op == 2) return {};
    if (op == 1) return {next + qint8(arg) * 2};
    if (comparison(op) || op == 0x2d) return {next, next + qint8(arg) * 2};
    return {next};
}

// Static interpretation of the script, never a snapshot of gameplay. A value
// survives a join only if all incoming paths agree, including its producers.
// Unrecognized instructions invalidate knowledge rather than inventing meaning.
struct Value {
    enum Kind { Unknown, Constant, Flag, Property, Dialogue } kind = Unknown;
    int number = -1, low = -1, high = -1;
    bool operator==(const Value &b) const {
        return kind == b.kind && number == b.number && low == b.low && high == b.high;
    }
};
using Registers = std::array<Value, 8>;
QString propertyName(int id) {
    switch (id) {
    case 0xff: return "Chaos Frame";
    case 0xe9: return "selected scenario";
    case 0xe7: return "staged scenario";
    default: return {};
    }
}
QString templateName(int id) {
    switch (id) {
    case 469: return "Ankiseth and four companions";
    case 526: return "Asnabel";
    case 527: return "Katreda";
    default: return {};
    }
}
QString templateLabel(int id) {
    const auto name = templateName(id);
    return QString("recruitment template %1%2").arg(id).arg(name.isEmpty() ? QString{} : " (" + name + ")");
}
QString scenarioLabel(int id) {
    const QString name = id == 7 ? "Gunther Piedmont: A New Beginning" :
        id == 20 ? "Wentinus: fight Ankiseth" : id == 21 ? "Wentinus: fight Baldwin" : QString{};
    return QString("scenario %1%2").arg(id).arg(name.isEmpty() ? QString{} : " (" + name + ")");
}
QString valueMeaning(const Value &value, int row) {
    switch (value.kind) {
    case Value::Constant: return QString::number(value.number);
    case Value::Flag: return EventScript::flagName(value.number, row);
    case Value::Property: return propertyName(value.number);
    case Value::Dialogue: return value.number == 3 ? "dialogue answer, control 3" : QString("dialogue control %1").arg(value.number);
    default: return {};
    }
}
QString registerLabel(int reg, const Registers &state, int row) {
    const auto meaning = valueMeaning(state[reg], row);
    return QString("R%1%2").arg(reg).arg(meaning.isEmpty() ? QString{} : " (" + meaning + ")");
}
Registers advance(const QByteArray &bytes, int at, Registers state) {
    const int op = u8(bytes, at), arg = u8(bytes, at + 1), reg = op & 7;
    if (op >= 0x68 && op <= 0x6f) state[reg] = {Value::Constant, arg, at, -1};
    else if (op >= 0x88 && op <= 0x8f) {
        const auto before = state[reg];
        state[reg] = before.kind == Value::Constant
            ? Value{Value::Constant, (before.number & 255) | (arg << 8), before.low, at} : Value{};
    } else if (op >= 0x60 && op <= 0x67) {
        const auto index = state[arg & 7];
        if (arg >= 0x60 && arg <= 0x67 && index.kind == Value::Constant && index.number <= 255)
            state[reg] = {Value::Flag, index.number, at, -1};
        else if (arg >= 0x90 && arg <= 0x97 && index.kind == Value::Constant)
            state[reg] = {Value::Dialogue, index.number, at, -1};
        else if (!propertyName(arg).isEmpty()) state[reg] = {Value::Property, arg, at, -1};
        else state[reg] = {};
    } else if ((op >= 0x70 && op <= 0x77) || op == 1 || comparison(op) || op == 0x2d) {
        // Setters consume registers; branch instructions only inspect them.
    } else state = {};
    return state;
}
QMap<int, Registers> registerInputs(const QByteArray &bytes, int entry, const QSet<int> &reachable) {
    QMap<int, Registers> inputs;
    inputs[entry] = {};
    QVector<int> pending{entry};
    while (!pending.isEmpty()) {
        const int at = pending.takeLast();
        const auto after = advance(bytes, at, inputs[at]);
        for (int next : successors(bytes, at)) {
            if (!reachable.contains(next)) continue;
            if (!inputs.contains(next)) { inputs[next] = after; pending << next; continue; }
            auto merged = inputs[next];
            for (int r = 0; r < 8; ++r) if (!(merged[r] == after[r])) merged[r] = {};
            if (merged != inputs[next]) { inputs[next] = merged; pending << next; }
        }
    }
    return inputs;
}
void annotate(QVector<EventStep> &steps, const QByteArray &bytes,
              const QMap<int, Registers> &inputs, int row) {
    QMap<int, QStringList> roles;
    auto describe = [&](const Value &value, const QString &role) {
        if (value.kind != Value::Constant || value.low < 0) return;
        const auto lowRole = value.high >= 0 ? "low byte of " + role : role;
        if (!roles[value.low].contains(lowRole)) roles[value.low] << lowRole;
        if (value.high >= 0 && !roles[value.high].contains(role)) roles[value.high] << role;
    };
    // Attribute prepared literals from the instruction that actually uses them.
    // Include instructions folded into the existing flag controls.
    for (auto it = inputs.cbegin(); it != inputs.cend(); ++it) {
        const int op = u8(bytes, it.key()), arg = u8(bytes, it.key() + 1);
        const auto &r = it.value();
        if (op >= 0x60 && op <= 0x67 && arg >= 0x60 && arg <= 0x67 &&
            r[arg & 7].kind == Value::Constant && r[arg & 7].number <= 255)
            describe(r[arg & 7], EventScript::flagName(r[arg & 7].number, row));
        if (op >= 0x60 && op <= 0x67 && arg >= 0x90 && arg <= 0x97 &&
            r[arg & 7].kind == Value::Constant)
            describe(r[arg & 7], r[arg & 7].number == 3 ? "dialogue answer control" : "dialogue control index");
        if (op >= 0x70 && op <= 0x77 && arg >= 0x60 && arg <= 0x67 &&
            r[arg & 7].kind == Value::Constant && r[arg & 7].number <= 255) {
            const auto name = EventScript::flagName(r[arg & 7].number, row);
            describe(r[arg & 7], name);
            if (r[op & 7].kind == Value::Constant)
                describe(r[op & 7], QString("%1 = %2").arg(name, r[op & 7].number == 1 ? "ON" : "OFF"));
        }
        if (op == 0x16 && arg <= 1 && r[0].kind == Value::Constant)
            describe(r[0], templateLabel(r[0].number));
        if (op == 0x10 && r[0].kind == Value::Constant)
            describe(r[0], QString("cutscene selector %1").arg(r[0].number));
        if (op >= 0x70 && op <= 0x77 && (arg == 0xe9 || arg == 0xe7) && r[op & 7].kind == Value::Constant)
            describe(r[op & 7], scenarioLabel(r[op & 7].number));
        if (op == 0xab) {
            if (r[3].kind == Value::Dialogue) describe(r[2], "comparison value for " + valueMeaning(r[3], row));
            if (r[2].kind == Value::Dialogue) describe(r[3], "comparison value for " + valueMeaning(r[2], row));
        }
    }
    for (auto &step : steps) {
        if (step.flag >= 0) continue;
        const int op = u8(bytes, step.offset), arg = u8(bytes, step.offset + 1), reg = op & 7;
        const auto r = inputs.value(step.offset);
        if (op >= 0x68 && op <= 0x6f) step.text = QString("Prepare R%1 = %2").arg(reg).arg(arg);
        else if (op >= 0x88 && op <= 0x8f) {
            step.text = QString("Prepare R%1 upper byte = %2").arg(reg).arg(arg);
            const auto after = advance(bytes, step.offset, r)[reg];
            if (after.kind == Value::Constant) step.text += QString(" → R%1 = %2").arg(reg).arg(after.number);
        } else if (op >= 0x60 && op <= 0x67) {
            const auto after = advance(bytes, step.offset, r)[reg];
            if (after.kind != Value::Unknown)
                step.text = QString("READ %1 → R%2").arg(valueMeaning(after, row)).arg(reg);
        } else if (op >= 0x70 && op <= 0x77 && arg >= 0x60 && arg <= 0x67 &&
                   r[arg & 7].kind == Value::Constant && r[arg & 7].number <= 255) {
            const auto name = EventScript::flagName(r[arg & 7].number, row);
            step.text = r[reg].kind == Value::Constant
                ? QString("%1 %2 = %3").arg(r[reg].number == 1 ? "SET" : "CLEAR", name, r[reg].number == 1 ? "ON" : "OFF")
                : QString("Write %1 from %2 (1 = ON; other values = OFF)").arg(name, registerLabel(reg, r, row));
        } else if (op >= 0x70 && op <= 0x77 && !propertyName(arg).isEmpty()) {
            step.text = QString("Write %1 from %2").arg(propertyName(arg), registerLabel(reg, r, row));
        } else if (op == 0x16 && arg <= 1) {
            step.text = QString("Queue %1 from %2").arg(arg == 0 ? "squad recruitment" : "individual recruitment",
                r[0].kind == Value::Constant ? templateLabel(r[0].number) : "R0 (recruitment template)");
        } else if (op == 0x13) {
            step.text = arg == 0x16 ? "Process queued army operation (request 22); then continue"
                                   : QString("Request operation %1; then continue").arg(arg);
        } else if (op == 0x11) {
            step.text = r[0].kind == Value::Constant
                ? QString("Request transition %1 (R0 low byte %2; operand %3)")
                    .arg((arg << 8) | (r[0].number & 255)).arg(r[0].number & 255).arg(arg)
                : QString("Request transition (R0 low byte; operand %1)").arg(arg);
            if (arg == 0 && r[0].kind == Value::Constant && (r[0].number & 255) == 1)
                step.text = "Finish this handler (transition request 1)";
        } else if (op == 0x12) {
            step.text = QString("Switch to event row %1").arg(arg);
            if (r[0].kind == Value::Constant) {
                const int first = r[0].number & 255, end = r[0].number >> 8;
                step.text += first < end ? QString("; CLEAR Flags %1–%2").arg(first).arg(end - 1)
                                        : "; no flags cleared";
            } else step.text += "; clear flag range prepared in R0";
        } else if (op == 0x10 && r[0].kind == Value::Constant) {
            step.text = QString("Play cutscene %1 (selector in R0)").arg(r[0].number);
        } else if (op == 0xac || op == 0xbc) {
            const auto test = r[0].kind == Value::Flag
                ? QString("%1 = %2").arg(EventScript::flagName(r[0].number, row), op == 0xbc ? "ON" : "OFF")
                : QString("%1 %2 0").arg(registerLabel(0, r, row), op == 0xbc ? "≠" : "=");
            step.text = QString("CHECK %1 → %2; otherwise → %3").arg(test, address(step.target), address(step.offset + 2));
        } else if (comparison(op)) {
            const QStringList relations{"=","≠",">","<","≥","≤"};
            const bool zero=(op&15)>=12;
            const int left=zero?op&3:(op>>2)&3,right=op&3;
            step.text = QString("CHECK %1 %2 %3 → %4; otherwise → %5")
                .arg(registerLabel(left,r,row),relations[(op>>4)-10],zero?QString("0"):registerLabel(right,r,row),
                     address(step.target),address(step.offset+2));
        }
        if (roles.contains(step.offset)) step.text += " (" + roles[step.offset].join("; ") + ")";
    }
}
}
QVector<EventHandler> EventScript::decode(const QByteArray &bytes, int eventRow) {
    if (bytes.size() < 8 || bytes.size() % 2) throw Error("Invalid event script");
    // Retail chapter introductions 110–114 have only the entry-handler group.
    // Mission programs have six groups; world routing uses direct entry pointers.
    const bool world=eventRow==0||eventRow==70;
    const int headerBytes = world?(eventRow==0?110:108):u16(bytes, 0) == 2 ? 2 : 12;
    if (bytes.size() < headerBytes) throw Error("Invalid event handler header");
    QVector<EventHandler> handlers;
    QSet<int> tableWords, entries, targets;
    if(const int directory=RetailEventGate::directory(bytes);directory>=0)
        for(int at=directory;at<u16(bytes,0);at+=2)tableWords.insert(at);
    for (int i = 0; i < headerBytes; i += 2) tableWords.insert(i);
    if(world) {
        for(int at=0;at<headerBytes;at+=2) {
            const int entry=u16(bytes,at);if(!entry||entry==65535)continue;
            if(entry<headerBytes||entry%2||entry>=bytes.size())throw Error("Invalid world event handler pointer");
            if(!entries.contains(entry)) {
                EventHandler handler{-1,entry,{}};handler.title=at?QString("Map location %1").arg(at/2):"New Game";
                handlers<<handler;entries.insert(entry);
            }
        }
    }else
    for (int group = 0; group < headerBytes / 2; ++group) {
        const int table = u16(bytes, group * 2);
        if (table < headerBytes || table % 2 || table >= bytes.size())
            throw Error("Event row does not use the supported handler layouts");
        bool ended = false;
        for (int at = table; at + 1 < bytes.size(); at += 2) {
            tableWords.insert(at);
            const int entry = u16(bytes, at);
            if (entry == 65535) { ended = true; break; }
            if (entry < headerBytes || entry % 2 || entry >= bytes.size()) throw Error("Invalid event handler pointer");
            handlers << EventHandler{group, entry, {}};
            entries.insert(entry);
        }
        if (!ended) throw Error("Unterminated event handler table");
    }
    QMap<int, QSet<int>> reachable;
    const auto nativeOffsets=eventRow>=0?instructionOffsets(eventRow):QSet<int>{};
    const int appendedFrom=nativeOffsets.isEmpty()?0x7fffffff:*std::max_element(nativeOffsets.begin(),nativeOffsets.end())+2;
    targets = entries;
    for (int entry : entries) {
        QVector<int> pending{entry};
        auto &seen = reachable[entry];
        while (!pending.isEmpty()) {
            const int at = pending.takeLast();
            if (seen.contains(at)) continue;
            if (at < headerBytes || at + 1 >= bytes.size() || at % 2 || tableWords.contains(at))
                throw Error("Event branch leaves its instruction area");
            seen.insert(at);
            const int op = u8(bytes, at);
            const auto next = successors(bytes, at, appendedFrom);
            for (int dest : next) {
                if (op == 1 || op==8 || ((comparison(op) || op == 0x2d) && dest != at + 2)) targets.insert(dest);
                pending << dest;
            }
        }
    }
    for (auto &handler : handlers) {
        auto offsets = reachable[handler.entry].values();
        std::sort(offsets.begin(), offsets.end());
        int until = -1;
        for (int at : offsets) {
            if (at < until) continue;
            EventStep step; step.offset = at;
            const int op = u8(bytes, at), arg = u8(bytes, at + 1);
            // A compound control may only own a straight-line sequence. Alternate
            // entries into its middle must remain separate instructions.
            const bool compound = at + 5 < bytes.size() && !targets.contains(at + 2) &&
                !targets.contains(at + 4) && reachable[handler.entry].contains(at + 4);
            if (compound && op == 0x68 && u8(bytes, at + 2) == 0x69 &&
                u8(bytes, at + 3) <= 1 && u16(bytes, at + 4) == 0x7160) {
                step.length = 6; step.flag = arg; step.value = u8(bytes, at + 3) != 0;
                step.text = QString("%1 %2 = %3").arg(step.value ? "SET" : "CLEAR", flagName(arg, eventRow), step.value ? "ON" : "OFF");
            } else if (compound && op == 0x69 && u16(bytes, at + 2) == 0x6061 &&
                       (u8(bytes, at + 4) == 0xac || u8(bytes, at + 4) == 0xbc)) {
                step.length = 6; step.flag = arg; step.check = true; step.value = u8(bytes, at + 4) == 0xbc;
                step.target = at + 6 + qint8(u8(bytes, at + 5)) * 2;
                step.text = QString("CHECK %1 = %2 → %3; otherwise → %4")
                    .arg(flagName(arg, eventRow), step.value ? "ON" : "OFF", address(step.target), address(at + 6));
            } else if (op == 1 || comparison(op) || op == 0x2d) {
                step.target = at + 2 + qint8(arg) * 2;
                step.text = QString("%1 → %2").arg(op == 1 ? "Go to" : "Conditional branch", address(step.target));
            } else if(op==8&&RetailEventGate::destination(bytes,at)) {
                step.target=*RetailEventGate::destination(bytes,at);
                step.text="Continue at "+address(step.target);
            } else if (op >= 0x68 && op <= 0x6f) {
                step.text = QString("Prepare value %1 in register %2").arg(arg).arg(op & 7);
            } else if (op >= 0x88 && op <= 0x8f) {
                step.text = QString("Prepare upper byte %1 in register %2").arg(arg).arg(op & 7);
            } else if (op == 0x10) step.text = "Play cutscene (prepared scene selector)";
            else if (op == 0x11 || op == 0x12) step.text = "Switch event script (prepared event row)";
            else if (op == 0x2f) step.text = "Finish this handler";
            else if (op >= 0x70 && op <= 0x77 && (arg == 0xe9 || arg == 0xe7))
                step.text = arg == 0xe9 ? "Select mission scenario" : "Stage mission scenario for loading";
            else step.text = QString("Game command %1 · operand %2")
                .arg(op, 2, 16, QChar('0')).arg(arg, 2, 16, QChar('0'));
            handler.steps << step;
            until = at + step.length;
        }
        annotate(handler.steps, bytes, registerInputs(bytes, handler.entry, reachable[handler.entry]), eventRow);
    }
    return handlers;
}
QMap<int, EventStep> EventScript::flagSteps(const QByteArray &bytes) {
    QMap<int, EventStep> result;
    for (const auto &handler : decode(bytes))
        for (const auto &step : handler.steps)
            if (step.flag >= 0) result[step.offset] = step;
    return result;
}
QByteArray EventScript::apply(const QByteArray &bytes, const QJsonObject &edits) {
    if (edits.isEmpty()) return bytes;
    const auto controls = flagSteps(bytes);
    const auto branches = branchSteps(bytes);
    QSet<int> destinations;
    for (const auto &handler : decode(bytes))
        for (const auto &step : handler.steps) destinations.insert(step.offset);
    auto result = bytes;
    for (auto it = edits.begin(); it != edits.end(); ++it) {
        bool ok; const int at = it.key().toInt(&ok);
        const auto edit = it.value().toObject();
        if (!ok || QString::number(at) != it.key() || !it.value().isObject() || edit.isEmpty())
            throw Error("Invalid event instruction edit");
        for (const auto &field : edit.keys())
            if (field != "flag" && field != "value" && field != "target") throw Error("Unknown event edit field");
        if (edit.contains("flag") || edit.contains("value")) {
            const int flag = edit["flag"].toInt(-1);
            if (!controls.contains(at) || flag < 0 || flag > 255 ||
                edit["flag"].toDouble() != flag || !edit["value"].isBool()) throw Error("Invalid event flag edit");
            put8(result, at + 1, quint8(flag));
            if (controls[at].check) put8(result, at + 4, edit["value"].toBool() ? 0xbc : 0xac);
            else put8(result, at + 3, edit["value"].toBool() ? 1 : 0);
        }
        if (edit.contains("target")) {
            const int target = edit["target"].toInt(-1);
            if (!branches.contains(at) || !destinations.contains(target) || edit["target"].toDouble() != target)
                throw Error("Choose a decoded instruction as the branch destination");
            const int end = at + branches[at].length, distance = (target - end) / 2;
            if (distance < -128 || distance > 127 || (target - end) % 2)
                throw Error("This branch destination is beyond the game's relative jump range");
            put8(result, end - 1, quint8(distance));
        }
    }
    decode(result);
    return result;
}
QMap<int, EventStep> EventScript::branchSteps(const QByteArray &bytes) {
    QMap<int, EventStep> result;
    for (const auto &handler : decode(bytes))
        for (const auto &step : handler.steps)
            if (step.target >= 0) result[step.offset] = step;
    return result;
}
} // namespace lordly
