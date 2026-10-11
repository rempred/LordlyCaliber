#pragma once
#include "engine/custom_squads.h"

namespace lordly {
// Cells are zero-based here; the original enemy template stores cells 1..9.
QVector<CustomMember> squadMembers(const QByteArray &record);
QByteArray editSquadMembers(const QByteArray &original, const QVector<CustomMember> &members);
QByteArray readSquadRecord(const QByteArray &library, int oneBasedId);
bool independentSquad(const QByteArray &record);
void validateSquadFormation(const QVector<CustomMember> &members, const ClassData &classes);
}
