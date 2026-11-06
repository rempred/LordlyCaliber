#include "engine/campaign_script.h"
#include "engine/event_script.h"
#include "engine/scene_events.h"
#include <QJsonArray>
#include <QSet>
namespace lordly {
namespace {
int integer(const QJsonValue &v, int low, int high) {
    const int n = v.toInt(low - 1);
    if (!v.isDouble() || v.toDouble() != n || n < low || n > high)
        throw Error("Invalid campaign script value");
    return n;
}
void word(QByteArray &b, int op, int value) {
    b += char(op);
    b += char(value);
}
} // namespace
QByteArray compileWorldEntries(const QByteArray &source, const QJsonObject &edits) {
    if (edits.isEmpty())
        return source;
    auto result = source;
    for (auto it = edits.begin(); it != edits.end(); ++it) {
        bool ok;
        const int node = it.key().toInt(&ok);
        if (!ok || QString::number(node) != it.key() || node < 1 || node > 40 ||
            !it.value().isArray() || it.value().toArray().isEmpty())
            throw Error("Invalid world-node entry rules");
        if (result.size() > 65535)
            throw Error("World-entry script exceeds its native pointer range");
        put16(result, node * 2, quint16(result.size()));
        const auto rules = it.value().toArray();
        if (rules.size() > SceneEvents::customRowCapacity)
            throw Error(QString("A node supports up to %1 entry rules").arg(SceneEvents::customRowCapacity));
        for (const auto &v : rules) {
            const auto rule = v.toObject();
            if (!v.isObject() || rule.size() != 4 || !rule["conditions"].isArray())
                throw Error("Invalid world-entry rule");
            const int event = integer(rule["event"], 1, SceneEvents::rowCapacity - 1),
                      from = integer(rule["clearFrom"], 0, 255),
                      to = integer(rule["clearTo"], 0, 255);
            if (from > to)
                throw Error("Flag-clear range must be in ascending order");
            const auto conditions = rule["conditions"].toArray();
            if (conditions.size() > 16)
                throw Error("A rule supports up to 16 flag conditions");
            QByteArray body;
            QSet<int> flags;
            QVector<int> skipOperands;
            for (int n = 0; n < conditions.size(); ++n) {
                const auto condition = conditions[n].toObject();
                if (condition.size() != 2 || !condition["value"].isBool())
                    throw Error("Invalid route condition");
                const int flag = integer(condition["flag"], 0, 767);
                if (flags.contains(flag))
                    throw Error("A flag can only be checked once per route");
                flags.insert(flag);
                word(body, 0x69, flag);
                if(flag>255)word(body,0x89,flag>>8);
                word(body, 0x60, 0x61);
                word(body, condition["value"].toBool() ? 0xac : 0xbc,0);
                skipOperands.append(int(body.size()-1));
            }
            word(body, 0x68, from);
            word(body, 0x88, to);
            word(body, 0x12, event);
            for(int operand:skipOperands)body[operand]=char((body.size()-operand-1)/2);
            result += body;
        }
        const int previous = u16(source, node * 2);
        if (previous) {
            // Native long relative jump: prepare its low byte in r0, then
            // opcode02 carries the upper byte (also used by scene launch stubs).
            // Retail entry rules initialize r0/r1 before reading them. Require
            // that property before appending a custom conditional prefix.
            QVector<QPair<int, QSet<int>>> pending{{previous, {}}};
            QSet<QPair<int, int>> seen;
            while (!pending.isEmpty()) {
                auto [pc, ready] = pending.takeLast();
                int mask = 0;
                for (int r : ready)
                    mask |= 1 << r;
                const auto state = qMakePair(pc, mask);
                if (seen.contains(state))
                    continue;
                seen.insert(state);
                if (pc < 110 || pc + 2 > source.size())
                    throw Error("Invalid original world-entry branch");
                const int op = u8(source, pc), arg = u8(source, pc + 1);
                if (op >= 0x68 && op <= 0x6f)
                    ready.insert(op & 7);
                else if (op >= 0x60 && op <= 0x67) {
                    if (arg == 0x61 && !ready.contains(1))
                        throw Error("Original world-entry uses an inherited register");
                    ready.insert(op & 7);
                } else if (op >= 0x88 && op <= 0x8f) {
                    if (!ready.contains(op & 7))
                        throw Error("Original world-entry uses an inherited register");
                } else if (op == 0x12) {
                    if (!ready.contains(0))
                        throw Error("Original world-entry uses an inherited register");
                    continue;
                } else if (op == 0x2f)
                    continue;
                else if (op >= 0xa0 && op <= 0xef) {
                    if (!ready.contains(0) || ((op == 0xa1 || op == 0xc1) && !ready.contains(1)))
                        throw Error("Original world-entry uses an inherited register");
                    pending << qMakePair(pc + 2 + qint8(arg) * 2, ready);
                } else if (op == 1) {
                    pending << qMakePair(pc + 2 + qint8(arg) * 2, ready);
                    continue;
                } else if (op == 0x71 && arg == 0x60) {
                    if (!ready.contains(0) || !ready.contains(1))
                        throw Error("Original world-entry uses an inherited register");
                } else if (op == 0x91 && arg == 2) {
                    if (!ready.contains(1) || !ready.contains(2))
                        throw Error("Original world-entry uses an inherited register");
                } else
                    throw Error(
                        "Original world-entry needs an unsupported command before the fallback");
                pending << qMakePair(pc + 2, ready);
            }
            const int delta = (previous - (result.size() + 4)) / 2;
            if (delta < -32768 || delta > 32767)
                throw Error("World-entry fallback exceeds its jump range");
            word(result, 0x68, delta & 255);
            word(result, 2, (delta >> 8) & 255);
        } else
            word(result, 0x2f, 0);
    }
    if (result.size() > 65535)
        throw Error("World-entry script exceeds its native pointer range");
    return result;
}
QByteArray appendWorldUnlocks(const QByteArray &source, const QJsonObject &unlocks) {
    if (unlocks.isEmpty())
        return source;
    EventScript::decode(source);
    auto result = source;
    // Add a completion handler by copying its pointer table; original code and
    // relative branches remain at the same addresses.
    QVector<int> entries;
    int at = u16(source, 10);
    while (u16(source, at) != 65535) {
        entries << u16(source, at);
        at += 2;
    }
    const int handler = result.size();
    for (auto it = unlocks.begin(); it != unlocks.end(); ++it) {
        bool ok;
        const int node = it.key().toInt(&ok);
        if (!ok || QString::number(node) != it.key() || node < 1 || node > 40 ||
            !it.value().isBool() || !it.value().toBool())
            throw Error("Invalid world-node unlock");
        word(result, 0x68, node);
        word(result, 0x69, 2);
        word(result, 0x71, 0xb0);
        word(result, 0x69, 1);
        word(result, 0x81, 0x50);
    }
    word(result, 0x2f, 0);
    const int table = result.size();
    entries.prepend(handler);
    for (int entry : entries) {
        word(result, entry >> 8, entry & 255);
    }
    word(result, 255, 255);
    if (result.size() > 65535)
        throw Error("Event script exceeds its native pointer range");
    put16(result, 10, quint16(table));
    EventScript::decode(result);
    return result;
}
} // namespace lordly
