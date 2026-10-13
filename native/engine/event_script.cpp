#include "engine/event_script.h"
#include <QMap>
#include <algorithm>
namespace lordly {
QString EventScript::flagName(int flag) {
    static const QMap<int, QString> names{{1, "Chapter One reached"}, {2, "Chapter Two reached"},
        {3, "Chapter Three reached"}, {4, "Final Chapter reached"}, {102, "Tenne Plains completed"}};
    return names.contains(flag) ? QString("%1 [flag %2]").arg(names[flag]).arg(flag)
                                : QString("Flag %1").arg(flag);
}
QString EventScript::groupName(int group) {
    return group == 0 ? "Mission entry" : group == 5 ? "Mission completion"
                                                    : QString("Event group %1").arg(group);
}
namespace {
QString address(int at) { return QString("@%1").arg(at, 4, 16, QChar('0')).toUpper(); }
QVector<int> successors(const QByteArray &bytes, int at) {
    const int op = u8(bytes, at), arg = u8(bytes, at + 1), next = at + 2;
    if (op == 0x11 || op == 0x12 || op == 0x2f || op == 0xff || op == 2) return {};
    if (op == 1) return {next + qint8(arg) * 2};
    if (op >= 0xa0 && op <= 0xef) return {next, next + qint8(arg) * 2};
    return {next};
}
}
QVector<EventHandler> EventScript::decode(const QByteArray &bytes) {
    if (bytes.size() < 12 || bytes.size() % 2) throw Error("Invalid mission event script");
    QVector<EventHandler> handlers;
    QSet<int> tableWords, entries, targets;
    for (int i = 0; i < 12; i += 2) tableWords.insert(i);
    for (int group = 0; group < 6; ++group) {
        const int table = u16(bytes, group * 2);
        if (table < 12 || table % 2 || table >= bytes.size())
            throw Error("Event row does not use the mission handler layout");
        bool ended = false;
        for (int at = table; at + 1 < bytes.size(); at += 2) {
            tableWords.insert(at);
            const int entry = u16(bytes, at);
            if (entry == 65535) { ended = true; break; }
            if (entry < 12 || entry % 2 || entry >= bytes.size()) throw Error("Invalid event handler pointer");
            handlers << EventHandler{group, entry, {}};
            entries.insert(entry);
        }
        if (!ended) throw Error("Unterminated event handler table");
    }
    QMap<int, QSet<int>> reachable;
    targets = entries;
    for (int entry : entries) {
        QVector<int> pending{entry};
        auto &seen = reachable[entry];
        while (!pending.isEmpty()) {
            const int at = pending.takeLast();
            if (seen.contains(at)) continue;
            if (at < 0 || at + 1 >= bytes.size() || at % 2 || tableWords.contains(at))
                throw Error("Event branch leaves its instruction area");
            seen.insert(at);
            const int op = u8(bytes, at);
            const auto next = successors(bytes, at);
            for (int dest : next) {
                if (op == 1 || (op >= 0xa0 && op <= 0xef && dest != at + 2)) targets.insert(dest);
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
            // entries into its middle must remain separate, read-only instructions.
            const bool compound = at + 5 < bytes.size() && !targets.contains(at + 2) &&
                !targets.contains(at + 4) && reachable[handler.entry].contains(at + 4);
            if (compound && op == 0x68 && u8(bytes, at + 2) == 0x69 &&
                u8(bytes, at + 3) <= 1 && u16(bytes, at + 4) == 0x7160) {
                step.length = 6; step.flag = arg; step.value = u8(bytes, at + 3) != 0;
                step.text = QString("%1 %2").arg(step.value ? "Set" : "Clear", flagName(arg));
            } else if (compound && op == 0x69 && u16(bytes, at + 2) == 0x6061 &&
                       (u8(bytes, at + 4) == 0xac || u8(bytes, at + 4) == 0xbc)) {
                step.length = 6; step.flag = arg; step.check = true; step.value = u8(bytes, at + 4) == 0xbc;
                step.target = at + 6 + qint8(u8(bytes, at + 5)) * 2;
                step.text = QString("If %1 is %2 → %3; otherwise → %4")
                    .arg(flagName(arg), step.value ? "ON" : "OFF", address(step.target), address(at + 6));
            } else if (op == 1 || (op >= 0xa0 && op <= 0xef)) {
                step.target = at + 2 + qint8(arg) * 2;
                step.text = QString("%1 → %2").arg(op == 1 ? "Go to" : "Conditional branch", address(step.target));
            } else if (op >= 0x68 && op <= 0x6f) {
                step.text = QString("Prepare value %1 in register %2").arg(arg).arg(op & 7);
            } else if (op >= 0x88 && op <= 0x8f) {
                step.text = QString("Prepare upper byte %1 in register %2").arg(arg).arg(op & 7);
            } else if (op == 0x10) step.text = "Play cutscene (prepared scene selector)";
            else if (op == 0x11 || op == 0x12) step.text = "Switch event script (prepared event row)";
            else if (op == 0x2f) step.text = "Finish this handler";
            else if (op >= 0x70 && op <= 0x77 && (arg == 0xe9 || arg == 0xe7))
                step.text = arg == 0xe9 ? "Select mission scenario" : "Stage mission scenario for loading";
            else step.text = QString("Game command %1 · operand %2 (read-only)")
                .arg(op, 2, 16, QChar('0')).arg(arg, 2, 16, QChar('0'));
            handler.steps << step;
            until = at + step.length;
        }
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
    auto result = bytes;
    for (auto it = edits.begin(); it != edits.end(); ++it) {
        bool ok; const int at = it.key().toInt(&ok);
        const auto edit = it.value().toObject();
        const int flag = edit["flag"].toInt(-1);
        if (!ok || QString::number(at) != it.key() || !controls.contains(at) || !it.value().isObject() ||
            edit.size() != 2 || flag < 0 || flag > 255 || edit["flag"].toDouble() != flag || !edit["value"].isBool())
            throw Error("Invalid event flag edit");
        put8(result, at + 1, quint8(flag));
        if (controls[at].check) put8(result, at + 4, edit["value"].toBool() ? 0xbc : 0xac);
        else put8(result, at + 3, edit["value"].toBool() ? 1 : 0);
    }
    return result;
}
} // namespace lordly
