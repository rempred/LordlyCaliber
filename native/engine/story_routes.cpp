#include "engine/story_routes.h"
#include <algorithm>
#include <array>
namespace lordly {
StoryRoutes::StoryRoutes(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Scenario seeding requires a US revision-zero ROM");
    const auto &b = rom.bytes();
    const int size = int(u32(b, 0x9398e8));
    if (size % 4 || size < 4 || size > 0x1000)
        throw Error("Unsupported event directory");
    for (int row = 0; row < size / 4; ++row) {
        const auto key = u32(b, 0x9398ec + row * 4);
        if (!key)
            continue;
        const quint64 at = 0x594280ULL + key;
        const auto length = u32(b, at), decoded = u32(b, at + 4);
        if (length < 4 || length > 0x1000000 || decoded > 0x1000000 ||
            at + 4 + length > quint64(b.size()))
            throw Error("Event resource exceeds its ROM envelope");
        qsizetype consumed = 0;
        auto script = decodeLzss(b.mid(at + 8, length - 4), decoded, &consumed);
        if (consumed != length - 4)
            throw Error("Event resource size mismatch");
        scripts[row] = script;
        QSet<int> selected;
        for (int i = 0; i + 5 < script.size(); i += 2)
            if (u8(script, i) == 0x68 && script.mid(i + 2, 4) == QByteArray::fromHex("70e970e7"))
                selected << u8(script, i + 1);
        keys[row] = selected.values();
        std::sort(keys[row].begin(), keys[row].end());
    }
    if (!scripts.contains(0))
        throw Error("Missing world-entry script");
    for (int node = 1; node < 54; ++node) {
        const int at = 0x864a0 + (node - 1) * 8;
        const qint64 text = qint64(u32(b, at + 4)) - 0x80121070LL;
        if (text < 0 || text >= b.size() || u8(b, text) != 14)
            continue;
        const auto end = b.indexOf(char(15), text);
        if (end < 0 || end - text > 256)
            throw Error("Unterminated world-node name");
        nodes[node] = {
            QString::fromLatin1(
                b.mid(text + 1, end - text - 1).replace(QByteArray::fromHex("1034"), " ")),
            {qint16(u16(b, at)), qint16(u16(b, at + 2))}};
    }
    for (int at = 0x858e4; at < 0x85934; at += 2) {
        const int a = u8(b, at), c = u8(b, at + 1);
        edges << qMakePair(a, c);
        if (nodes.contains(a))
            travelNodes << a;
        if (nodes.contains(c))
            travelNodes << c;
    }
}
QVector<StoryRoute> StoryRoutes::routes(int node, int chaos, QMap<int, int> flags) const {
    if (!nodes.contains(node) || chaos < 0 || chaos > 100)
        throw Error("Invalid story entry");
    const auto &world = scripts[0];
    const int start = u16(world, node * 2);
    if (!start)
        return {};
    struct State {
        int pc, steps = 0;
        std::array<int, 8> regs{};
        QMap<int, int> known, required, writes;
    };
    QVector<State> pending{{start, 0, {}, flags, {}, {}}};
    QVector<StoryRoute> output;
    int total = 0;
    while (!pending.isEmpty()) {
        auto s = pending.takeLast();
        auto &r = s.regs;
        for (;;) {
            if (++s.steps > 1000 || ++total > 100000 || s.pc < 0 || s.pc + 2 > world.size())
                throw Error("Invalid or looping world-entry script");
            const int op = u8(world, s.pc), arg = u8(world, s.pc + 1);
            bool jump = false;
            if (op >= 0x68 && op <= 0x6f)
                r[op & 7] = arg;
            else if (op >= 0x88 && op <= 0x8f)
                r[op & 7] = (r[op & 7] & 255) | (arg << 8);
            else if (op >= 0x60 && op <= 0x67) {
                if (arg == 0x61) {
                    const int flag = r[1];
                    if (flag < 0 || flag >= 256)
                        throw Error("Unsupported world-entry flag");
                    if (!s.known.contains(flag)) {
                        for (int value : {0, 1}) {
                            auto next = s;
                            next.known[flag] = value;
                            next.required[flag] = value;
                            pending << next;
                        }
                        break;
                    }
                    r[op & 7] = s.known[flag];
                } else if (arg == 0xfa)
                    r[op & 7] = 1;
                else if (arg == 0xf9)
                    r[op & 7] = node;
                else if (arg == 0xff)
                    r[op & 7] = chaos;
                else
                    throw Error("Unsupported world-entry property");
            } else if (op == 0x71 && arg == 0x60)
                s.known[r[0]] = s.writes[r[0]] = bool(r[1]);
            else if (op == 0x91 && arg == 2)
                r[1] &= r[2];
            else if (op == 1)
                jump = true;
            else if (op == 0xac)
                jump = r[0] == 0;
            else if (op == 0xa1)
                jump = r[0] == r[1];
            else if (op == 0xc1)
                jump = r[1] < r[0];
            else if (op == 0xcc)
                jump = r[0] > 0 && r[0] < 0x8000;
            else if (op == 0x12) {
                StoryRoute route{node, arg, r[0] & 255, r[0] >> 8, s.required, s.writes};
                if (!output.contains(route))
                    output << route;
                break;
            } else if (op == 0x2f)
                break;
            else
                throw Error(QString("Unsupported world-entry command %1").arg(op, 2, 16));
            s.pc += 2 + (jump ? 2 * qint8(arg) : 0);
        }
    }
    return output;
}
namespace {
QVector<int> journey(int row) {
    QVector<int> early{4, 5, 110, 6, 7, 8, 69, 9, 10, 11, 12, 111, 13, 14, 15};
    if (early.contains(row))
        return early.mid(0, early.indexOf(row));
    const QMap<int, QVector<int>> first{{16, {}},
                                        {18, {}},
                                        {17, {16}},
                                        {19, {16, 17}},
                                        {20, {16, 17, 19}},
                                        {21, {16, 17, 19}},
                                        {22, {16, 17, 19, 20}},
                                        {23, {16, 17, 19, 20}},
                                        {24, {16, 17, 19, 20}}};
    if (first.contains(row))
        return early + first[row];
    auto second = early + QVector<int>{16, 17, 19, 20, 23, 112, 25, 26, 27};
    if (row == 25 || row == 26 || row == 27)
        return second.mid(0, second.indexOf(row));
    const QMap<int, QVector<int>> paths{{28, {}},       {29, {}},           {35, {}},
                                        {30, {29, 28}}, {31, {28}},         {32, {28, 31}},
                                        {36, {35}},     {33, {28, 31, 32}}, {34, {28, 31, 32, 33}}};
    if (paths.contains(row))
        return second + paths[row];
    auto third = second + QVector<int>{28, 31, 32, 33, 34, 114, 37};
    if (row == 37)
        return third.mid(0, third.size() - 1);
    if (row == 38 || row == 39)
        return third;
    auto fourth = third + QVector<int>{38, 40};
    if (row == 40)
        return fourth.mid(0, fourth.size() - 1);
    const QMap<int, QVector<int>> late{{41, {}},
                                       {43, {}},
                                       {42, {41}},
                                       {44, {41, 42}},
                                       {45, {41, 42, 43, 44}},
                                       {46, {41}},
                                       {47, {41, 42, 43, 44, 45}},
                                       {48, {41, 46}},
                                       {49, {41, 46}},
                                       {50, {41, 42, 43, 44, 45, 47}},
                                       {51, {41, 42, 43, 44, 45, 47}},
                                       {58, {43, 44, 42, 47, 50}},
                                       {60, {41, 46, 48}},
                                       {59, {41, 43, 44, 47, 50}},
                                       {61, {41, 42, 46, 48}}};
    if (late.contains(row))
        return fourth + late[row];
    auto final = fourth + QVector<int>{41, 42, 43, 44, 45, 47, 50, 52, 53, 54, 55, 56};
    if (row >= 52 && row <= 56)
        return final.mid(0, final.indexOf(row));
    if (row == 57)
        return final;
    throw Error(QString("No story-road journey is defined for event %1").arg(row));
}
} // namespace
WorldProgress::WorldProgress(const StoryRoutes &routing, const QByteArray &memory)
    : routing_(routing), memory_(memory) {
    for (int i = 0; i < 256; ++i)
        flags[i] = (u8(memory, 0x193686 + i / 8) >> (i % 8)) & 1;
    for (int i = 0; i < 40; ++i)
        roads_[i + 1] = (u8(memory, 0x196a81 + i / 8) >> (i % 8)) & 1;
    for (int i = 1; i < 53; ++i)
        nodes_[i] = (u8(memory, 0x196a86 + (i - 1) / 4) >> (((i - 1) % 4) * 2)) & 3;
    for (int i = 1; i < 49; ++i)
        markers_[i] = (u8(memory, 0x196a93 + (i - 1) / 8) >> ((i - 1) % 8)) & 1;
}
void WorldProgress::applyList(const QVector<int> &values) {
    std::optional<QPair<int, int>> previous;
    for (int i = 0; i < values.size();) {
        int node = values[i++];
        if (node == 255) {
            previous.reset();
            continue;
        }
        if (i == values.size())
            throw Error("Incomplete world-road command");
        int mode = values[i++];
        if (!nodes_.contains(node) || mode < 0 || mode > 6)
            throw Error("Unsupported world-road command");
        if (mode <= 2)
            nodes_[node] = mode;
        else if (mode == 5 || mode == 6)
            markers_[node] = mode == 6;
        if (previous) {
            auto pair = qMakePair(previous->first, node);
            int edge = routing_.edges.indexOf(pair);
            if (edge < 0)
                edge = routing_.edges.indexOf(qMakePair(node, previous->first));
            if (edge < 0)
                throw Error("Story journey references a road absent from this ROM");
            roads_[edge + 1] = previous->second != 0 && previous->second != 4;
        }
        previous = qMakePair(node, mode);
    }
}
void WorldProgress::complete(int row) {
    if (!routing_.scripts.contains(row))
        throw Error("Missing story completion script");
    const auto &s = routing_.scripts[row];
    int state = QVector<int>{69, 110, 111, 112, 114}.contains(row) ? 0 : 5;
    int table = u16(s, state * 2);
    QVector<int> starts;
    while (table + 2 <= s.size()) {
        int start = u16(s, table);
        table += 2;
        if (start == 0xffff)
            break;
        starts << start;
    }
    if (starts.isEmpty())
        return;
    int first = *std::min_element(starts.begin(), starts.end());
    if (first < 0 || first >= s.size())
        throw Error("Invalid story callback");
    QVector<int> stores;
    for (int pc = first; pc + 5 < s.size(); pc += 2)
        if (u8(s, pc) == 0x68 && u8(s, pc + 2) == 0x69 &&
            (s.mid(pc + 4, 2) == QByteArray::fromHex("71b0") ||
             s.mid(pc + 4, 2) == QByteArray::fromHex("71b8") ||
             s.mid(pc + 4, 2) == QByteArray::fromHex("8150")))
            stores << pc;
    int pc = stores.isEmpty() ? starts.last() : stores.first();
    int reset = int(s.indexOf(QByteArray::fromHex("6800692798416a0072b0b1fc"), first));
    if (reset >= 0)
        pc = reset;
    std::array<int, 8> r{};
    QVector<int> values;
    int steps = 0;
    while (pc >= 0 && pc + 2 <= s.size()) {
        if (++steps > 10000)
            throw Error("World progression loop");
        int op = u8(s, pc), arg = u8(s, pc + 1);
        bool jump = false;
        if (op >= 0x68 && op <= 0x6f)
            r[op & 7] = arg;
        else if (op >= 0x88 && op <= 0x8f)
            r[op & 7] = (r[op & 7] & 255) | (arg << 8);
        else if (op == 0x60 && arg == 0x61)
            r[0] = flags.value(r[1]);
        else if (op == 0x61 && arg == 0xb8)
            r[1] = roads_.value(r[0]);
        else if (op == 0x60 && arg == 0xff) {
            if (!stores.isEmpty())
                break;
        } else if (op == 0x71 && arg == 0x60)
            flags[r[0]] = bool(r[1]);
        else if (op >= 0x70 && op <= 0x77 && arg == 0xb0)
            nodes_[r[0]] = r[op & 7];
        else if (op >= 0x70 && op <= 0x77 && arg == 0xb8)
            roads_[r[0]] = bool(r[op & 7]);
        else if (op == 0x81 && arg == 0x50)
            markers_[r[0]] = bool(r[1]);
        else if (op == 0x1b)
            values << arg;
        else if (op == 1)
            jump = true;
        else if (op == 0xac)
            jump = r[0] == 0;
        else if (op == 0xad)
            jump = r[1] == 0;
        else if (op == 0xbc)
            jump = r[0] != 0;
        else if (op == 0xbd)
            jump = r[1] != 0;
        else if (op == 0xb1)
            jump = r[0] != r[1];
        else if (op == 0x98 && arg == 0x41)
            ++r[0];
        else if (op == 0x11 || op == 0x2f || op == 0xff)
            break;
        else if (op >= 0xa0 && op <= 0xef) {
            if (!stores.isEmpty())
                throw Error("Unsupported world-progression condition");
            break;
        }
        pc += 2 + (jump ? 2 * qint8(arg) : 0);
    }
    applyList(values);
    const int end = stores.isEmpty() ? int(s.size()) : std::min(pc + 2, int(s.size()));
    for (int i = first; i + 5 < end; i += 2)
        if (u8(s, i) == 0x68 && u8(s, i + 1) >= 102 && u8(s, i + 1) <= 155 &&
            s.mid(i + 2, 4) == QByteArray::fromHex("69017160"))
            flags[u8(s, i + 1)] = 1;
    for (int key : routing_.keys[row])
        key_ = std::max(key_, key);
    int chapter = QMap<int, int>{{110, 1}, {111, 2}, {112, 3}, {114, 4}}.value(row);
    if (chapter &&
        s.contains(QByteArray::fromHex("680069017160").replace(1, 1, QByteArray(1, char(chapter)))))
        flags[chapter] = 1;
    if (row == 23 || row == 24) {
        flags[119] = 1;
        flags[19] = row == 23;
    }
    if (row >= 48 && row <= 51 && s.contains(QByteArray::fromHex("681d69017160")))
        flags[29] = 1;
}
void WorldProgress::advanceTo(int row) {
    for (int event : journey(row))
        complete(event);
}
QByteArray WorldProgress::forEntry(const StoryRoute &route) {
    advanceTo(route.event);
    for (auto i = route.requirements.begin(); i != route.requirements.end(); ++i)
        flags[i.key()] = i.value();
    if (route.event == 23 || route.event == 24)
        flags[119] = 1;
    nodes_[route.node] = 2;
    for (auto i = markers_.begin(); i != markers_.end(); ++i)
        i.value() = i.key() == route.node;
    bool connected = false;
    for (int n = 0; n < routing_.edges.size(); ++n)
        if (roads_.value(n + 1) &&
            (routing_.edges[n].first == route.node || routing_.edges[n].second == route.node))
            connected = true;
    if (!connected)
        throw Error("The story journey did not open a road to this mission");
    auto out = memory_;
    put8(out, 0x1936a7, quint8(key_));
    auto pack = [&](int at, int count, int bits, int first, const QMap<int, int> &data) {
        for (int i = 0; i < count; ++i) {
            int value = 0;
            for (int b = 0; b < 8 / bits; ++b)
                value |= data.value(first + i * (8 / bits) + b) << (b * bits);
            put8(out, at + i, quint8(value));
        }
    };
    pack(0x193686, 32, 1, 0, flags);
    pack(0x196a81, 5, 1, 1, roads_);
    pack(0x196a86, 13, 2, 1, nodes_);
    pack(0x196a93, 6, 1, 1, markers_);
    return out;
}
} // namespace lordly
