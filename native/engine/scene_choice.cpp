#include "engine/scene_actions.h"
namespace lordly {
QVector<SceneChoice> SceneActions::choices(const DirectorProgram &p) {
    const auto &c = p.commands;
    auto matches = [&](int at, quint32 op, QVector<quint32> args = {}) {
        return at >= 0 && at < c.size() && c[at].opcode == op && c[at].operands == args;
    };
    QVector<SceneChoice> result;
    for (int i = 0; i + 21 < c.size(); ++i) {
        if (c[i].opcode != 0xbf || c[i].operands[0] > 255 || c[i + 7].opcode != 0x59 ||
            c[i + 11].opcode != 0x59)
            continue;
        const auto id = c[i].operands[0], a = c[i + 7].operands[0], b = c[i + 11].operands[0];
        if (a == b || !matches(i + 1, 0) || !matches(i + 2, 0x80000002) ||
            !matches(i + 3, 0x80000000) || !matches(i + 4, 0x56) ||
            !matches(i + 5, 0x25, {0, 1, 3}) || !matches(i + 6, 5, {id}) ||
            !matches(i + 8, 0x80000002) || !matches(i + 9, 0x25, {0, 255, 3}) ||
            !matches(i + 10, 5, {id}) || !matches(i + 12, 0x80000002) ||
            !matches(i + 13, 0x80000000) || !matches(i + 14, 0x80000005, {a}) ||
            !matches(i + 15, 0x10, {0, 1, id}))
            continue;
        int endA = i + 16;
        while (endA + 3 < c.size() &&
               !(c[endA].opcode == 0x59 && matches(endA + 1, 0x80000005, {b})))
            ++endA;
        if (endA + 3 >= c.size() || !matches(endA + 2, 0x10, {0, 1, id}))
            continue;
        const auto join = c[endA].operands[0];
        if (join == a || join == b)
            continue;
        int endB = endA + 3;
        while (endB + 1 < c.size() &&
               !(matches(endB, 0x59, {join}) && matches(endB + 1, 0x80000005, {join})))
            ++endB;
        if (endB + 1 >= c.size())
            continue;
        // Do not absorb a label that unrelated native code enters or defines.
        bool privateLabels = true;
        for (auto label : {a, b, join}) {
            int definitions = 0, references = 0;
            for (const auto &command : c) {
                definitions += command.opcode == 0x80000005 && command.operands[0] == label;
                references += command.opcode == 0x59 && command.operands[0] == label;
            }
            privateLabels &= definitions == 1 && references == (label == join ? 2 : 1);
        }
        if (privateLabels)
            result << SceneChoice{i, i + 16, endA, endA + 3, endB, endB + 2};
    }
    return result;
}
QByteArray SceneActions::choiceText(const QString &first, const QString &second) {
    for (const auto &text : {first, second}) {
        if (text.trimmed().isEmpty() || text.size() > 24)
            throw Error("Each choice option needs 1 to 24 printable characters.");
        for (const auto ch : text)
            if (ch.unicode() < 32 || ch.unicode() > 126 || QString("@{}").contains(ch))
                throw Error(
                    "Choice options cannot contain control tokens or non-ASCII characters.");
    }
    return QByteArray("@S0@%0@n    1. {T05}") + first.trimmed().toLatin1() + "@n    2. {T05}" +
           second.trimmed().toLatin1() + "@1@w3001@c@n@2@w3255@c@n@#";
}
std::optional<QPair<QString, QString>> SceneActions::choiceOptions(const QByteArray &text) {
    const QByteArray prefix = QByteArray("@S0@%0@n    1. {T05}"),
                     separator = QByteArray("@n    2. {T05}"),
                     suffix = "@1@w3001@c@n@2@w3255@c@n@#";
    if (!text.startsWith(prefix) || !text.endsWith(suffix))
        return {};
    const auto middle = text.indexOf(separator, prefix.size());
    if (middle < 0)
        return {};
    const auto first = QString::fromLatin1(text.mid(prefix.size(), middle - prefix.size()));
    const auto second = QString::fromLatin1(text.mid(
        middle + separator.size(), text.size() - suffix.size() - middle - separator.size()));
    try {
        if (choiceText(first, second) == text)
            return QPair{first, second};
    } catch (const Error &) {
    }
    return {};
}
} // namespace lordly
