#include "engine/event_context.h"
#include "engine/event_script.h"
#include <algorithm>
namespace lordly {
EventContexts::EventContexts(const Rom &rom, const SceneEvents &events)
    : events_(events), routes_(rom) {
    routes_.scripts[0] = events_.program(0);
    routes_.scripts[70] = events_.program(70);
}
QString EventContexts::programName(int row) {
    const auto name = EventScript::rowName(row);
    const auto prefix = QString::number(row) + " · ";
    return name.startsWith(prefix) ? name.mid(prefix.size()) : name;
}
QVector<int> EventContexts::destinations(int location, int routingRow) const {
    auto routing = routes_;
    // Newly created project locations may not yet exist in the opened ROM's
    // map catalog. Their actual current entry pointer remains authoritative.
    if (!routing.nodes.contains(location)) routing.nodes[location] = {};
    routing.scripts[0] = routing.scripts.value(routingRow);
    QSet<int> found;
    // Native world rules compare Chaos Frame and flags. The bounded existing
    // interpreter forks unknown flags and preserves repeated-flag constraints;
    // evaluating each legal CF exposes every supported branch without assuming
    // which story route a user intends to edit.
    for (int chaos = 0; chaos <= 100; ++chaos)
        for (const auto &route : routing.routes(location, chaos)) found.insert(route.event);
    auto rows = found.values();
    std::sort(rows.begin(), rows.end());
    return rows;
}
EventLocationContext EventContexts::location(int id) const {
    if (id < 1 || id > 40) throw Error("Choose a map location");
    if (locations_.contains(id)) return locations_.value(id);
    EventLocationContext result; result.location = id;
    const auto &arrival = routes_.scripts[0];
    const int entry = u16(arrival, id * 2);
    // Empty New Campaign routes have a native return instruction so the
    // dispatcher never runs the pointer table as code.
    const auto hasEvent = [&](int row, const QByteArray &program, int offset) {
        return offset && (!events_.flagEdits()[QString::number(row)].toObject()["replace"].toBool() ||
                          u16(program, offset) != 0x2f00);
    };
    if (hasEvent(0, arrival, entry)) result.phases[0].append({0, entry, "Arrival"});
    for (const auto &[routingRow, phase] : {qMakePair(0, 1), qMakePair(70, 2)})
        for (int row : destinations(id, routingRow))
            result.phases[phase].append({row, -1, events_.missionName(row).isEmpty()?programName(row):events_.missionName(row)});
    const auto &investigation = routes_.scripts[70];
    const int investigationEntry = u16(investigation, id * 2);
    if (hasEvent(70, investigation, investigationEntry))
        result.phases[2].append({70, investigationEntry, "Entry conditions"});
    locations_[id] = result;
    return result;
}
QVector<int> EventContexts::locationsForProgram(int row,int offset)const{
    QVector<int> result;
    if(row==0||row==70){
        if(offset<0)return result;
        const auto bytes=routes_.scripts.value(row);QVector<int> starts;
        for(int at=0;at<(row==70?108:110);at+=2)if(u16(bytes,at))starts<<u16(bytes,at);
        std::sort(starts.begin(),starts.end());
        for(int id=1;id<=40;++id){
            const int first=u16(bytes,id*2);if(!first)continue;
            const auto next=std::upper_bound(starts.begin(),starts.end(),first);
            const int end=next==starts.end()?bytes.size():*next;
            if(offset>=first&&offset<end)result<<id;
        }
    }else for(int id=1;id<=40;++id){
        const auto context=location(id);bool matches=false;
        for(int phase:{1,2})for(const auto &choice:context.phases[phase])matches|=choice.row==row;
        if(matches)result<<id;
    }
    return result;
}
QVector<int> EventContexts::scenarioKeys(int row) const {
    if (keys_.contains(row)) return keys_.value(row);
    const auto bytes = events_.program(row);
    QSet<int> keys;
    // This is the native scenario initialization sequence already used by
    // SceneEvents::scenarioRows, read from current bytes so edited keys count.
    const auto suffix = QByteArray::fromHex("70e970e7");
    for (int at = 0; at + 6 <= bytes.size(); at += 2)
        if (u8(bytes, at) == 0x68 && bytes.mid(at + 2, 4) == suffix)
            keys.insert(u8(bytes, at + 1));
    auto result = keys.values();std::sort(result.begin(), result.end());
    keys_[row] = result;return result;
}
QVector<int> EventContexts::locationsForScenario(int key) const {
    QVector<int> result;
    if (key < 0 || key > 255) return result;
    for (int id = 1; id <= 40; ++id) {
        const auto context = location(id);
        bool matches = false;
        for (int phase : {1, 2}) for (const auto &program : context.phases[phase])
            matches |= scenarioKeys(program.row).contains(key);
        if (matches) result.append(id);
    }
    return result;
}
QVector<int> EventContexts::scenarioPrograms(int key) const {
    QVector<int> result;
    if (key < 0 || key > 255) return result;
    for (int row = 2; row < SceneEvents::rowCapacity; ++row)
        if (row != 70 && scenarioKeys(row).contains(key)) result.append(row);
    return result;
}
} // namespace lordly
