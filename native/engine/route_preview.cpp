#include "engine/route_preview.h"
#include "engine/squad_members.h"
#include "game/scenarios/route_path.h"
#include <QSet>
#include <algorithm>
#include <array>
#include <cmath>

namespace lordly {
namespace {
int movement(const Scenario &s, const ClassData &classes, const QByteArray &priority, int row) {
    const int source = (u8(s.layout.deployments[row], 0) & 127) + 1;
    // Accepted func_00129068: source-record ID is deployment index + 1.
    if ((source == 31 && s.key != 8) ||
        (source == 32 && QSet<int>{39,45,46,47,60}.contains(s.key))) return 7;
    QVector<int> types;
    for (const auto &m : squadMembers(s.squads[row])) {
        if (m.classId < 1 || m.classId >= classes.entries().size())
            throw Error("Route preview needs valid squad classes");
        const int type = m.classId == 1 ? 2 : u8(classes.entries()[m.classId].record, 44);
        if (type < 1 || type > 7) throw Error("A squad class has an unknown movement type");
        types << type;
    }
    for (auto p : priority) if (types.contains(quint8(p))) return quint8(p);
    return 7;
}
QPointF grid(const ScenarioMap &map, QPointF p) {
    return {(p.x() - map.bounds.left()) * 256. / map.bounds.width(),
            (p.y() - map.bounds.top()) * 256. / map.bounds.height()};
}
int cell(QPointF p) {
    if (!std::isfinite(p.x()) || !std::isfinite(p.y()) ||
        p.x() < 0 || p.y() < 0 || p.x() >= 256 || p.y() >= 256)
        throw Error("Route endpoint lies outside the mission map");
    return int(p.y() / 4) * 64 + int(p.x() / 4);
}
}
RoutePreview RoutePreview::squad(const Rom &rom, const Scenarios &scenarios,
                                const ClassData &classes, const MapEdits &maps,
                                int key, int row) {
    RoutePreview out;
    out.detail = "Planned terrain route from the starting position. The game smooths turns between "
                 "these path cells; enemies, evasion, time of day and scripted events can change it.";
    try {
        rom.requireSupported();
        if (rom.revision()) throw Error("Route preview requires US revision 0");
        if (sha256(rom.bytes().mid(0x107850, 0xca8)) !=
                "38e6510af444b5fefedf9c040ee2a0e3e6cf4d272ddf933d1a43477d13ced078" ||
            sha256(rom.bytes().mid(0x102da0, 0x94)) !=
                "cb7e39877ea44f44323e8c1a090cbb9d8f8ff8db66b7f036bdd0c7bc3d546e8d")
            throw Error("This ROM has changed movement code; its route behavior is not recognized");
        const auto &s = scenarios.scenario(key);
        const auto &deployments = s.layout.deployments;
        if (row < 0 || row >= deployments.size()) throw Error("Choose a squad");
        const auto priority = rom.bytes().mid(0x5cebc, 7);
        for (auto p : priority) if (quint8(p) < 1 || quint8(p) > 7)
            throw Error("Unrecognized squad movement priorities");
        int leader = row, end = row + 1;
        if (row && u8(deployments[row - 1], 16)) {
            const int group = u8(deployments[row - 1], 16);
            while (leader > 1 && u8(deployments[leader - 2], 16) == group) --leader;
            while (end < deployments.size() && u8(deployments[end - 1], 16) == group) ++end;
        }
        out.leaderRow = leader;
        QVector<int> types;
        for (int i = leader; i < end; ++i) types << movement(s, classes, priority, i);
        for (auto p : priority) if (types.contains(quint8(p))) { out.movementType = quint8(p); break; }
        if (leader != row || end != row + 1)
            out.detail.prepend(QString("Movement group: showing Squad %1's route; follower formation offsets vary. ")
                              .arg(u8(deployments[leader], 0) & 127));
        if (out.movementType < 1 || out.movementType > 7) throw Error("Unknown squad movement type");
        if (out.movementType == 7) { out.message = "This squad cannot move with its current setup."; return out; }
        auto map = maps.source(key).map;
        map.points = maps.points(key);
        const auto start = map.placement(u8(deployments[leader], 3), u8(deployments[leader], 4));
        if (!start) throw Error("Squad starting position is unresolved");
        QPointF position = grid(map, *start);
        const auto terrain = maps.movementTerrain(key);
        // Descriptor 7: ROM101CE0..145210 -> RAM801AD5A0..801F0AD0.
        // ROM102DA0..DB4 copies ROM13FA58 (RAM801EB318) into BSS801F36D8.
        std::array<uint16_t,32> costs;
        for (int i = 0; i < 32; ++i) costs[i] = u16(rom.bytes(), 0x13fa58 + out.movementType * 64 + i * 2);
        const auto transitions = rom.bytes().mid(0x13fc60, 1024);
        if (terrain.size() != 4096 || transitions.size() != 1024)
            throw Error("Movement terrain is unavailable");
        if (key == 13 || key == 17)
            out.detail += " This mission can close terrain passages through story events; the preview uses their initial state.";
        const auto boundary = maps.boundary(key);
        int node = u8(deployments[leader], 6);
        QSet<int> visited;
        QString stop;
        while (node >= 4 && node < s.layout.nodes.size() + 4) {
            if (visited.contains(node)) { stop = "Route repeats from here."; break; }
            visited.insert(node);
            const auto &n = s.layout.nodes[node - 4];
            const int kind = u8(n, 1);
            if (kind != 1) {
                stop = kind == 0 ? "Waits at this position until its orders change."
                                 : "Waiting for a scripted condition.";
                break;
            }
            for (int at : {4,6,8}) {
                if (!u8(n, at)) break;
                const auto destination = map.placement(u8(n, at), u8(n, at + 1));
                if (!destination) throw Error("Waypoint destination is unresolved");
                const auto to = grid(map, *destination);
                if (!boundary.contains(position) || !boundary.contains(to))
                    throw Error("Route endpoint lies outside the playable boundary");
                const int fromCell = cell(position), toCell = cell(to);
                if (!u8(terrain, fromCell) || !u8(terrain, toCell))
                    throw Error("Route endpoint is outside the movement terrain");
                uint16_t path[4096]; int count = 0; uint32_t cost = 0;
                const int found = lordly_route_path(
                    reinterpret_cast<const uint8_t *>(terrain.constData()), costs.data(),
                    reinterpret_cast<const uint8_t *>(transitions.constData()), nullptr,
                    fromCell, toCell, path, &count, &cost);
                if (found < 0) throw Error("Movement grid contains an unsupported terrain value");
                if (!found) { out.message = "No terrain route to the destination for this squad."; return out; }
                if (out.points.isEmpty()) out.points << position;
                for (int i = 0; i < count; ++i) {
                    out.cells << path[i];
                    // The native route is a cell chain, not a straight waypoint link.
                    if (i && i + 1 < count) out.points << QPointF((path[i] % 64) * 4 + 2, (path[i] / 64) * 4 + 2);
                }
                out.points << to;
                position = to;
            }
            if (n.mid(10, 7) != QByteArray(7, 0)) { stop = "Preview stops at the next scripted condition."; break; }
            node = u8(n, 17);
        }
        out.reachable = !out.points.isEmpty();
        out.message = out.reachable ? "Terrain route shown" : "No fixed destination in the starting orders.";
        if (!stop.isEmpty()) out.message += " · " + stop;
        const QStringList names{"None", "Sky", "Plain", "Forest", "Mountain", "Snow", "Marsh", "Immobile"};
        out.message += " · " + names[out.movementType] + " movement";
    } catch (const Error &e) {
        out.reachable = false; out.points.clear(); out.cells.clear();
        out.message = QString::fromUtf8(e.what());
    }
    return out;
}
}
