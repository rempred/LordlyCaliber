#pragma once
#include "engine/project.h"
namespace lordly {
inline QString encounterSquadName(const Project &project, int key) {
    const auto &profiles = project.encounters()->customSquads().profiles();
    const auto found = profiles.constFind(key);
    if (found == profiles.cend() || found->members.isEmpty()) return "Custom squad";
    const int leader = found->members.first().classId;
    int count = 0, number = 0;
    for (auto it = profiles.cbegin(); it != profiles.cend(); ++it)
        if (!it->members.isEmpty() && it->members.first().classId == leader) {
            ++count; if (it.key() == key) number = count;
        }
    return QString(project.record(Table::Classes, leader).name).replace('\n', ' ') +
           " Squad" + (count > 1 ? QString(" %1").arg(number) : QString());
}
}
