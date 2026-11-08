#pragma once
#include "engine/rom.h"
namespace lordly {
constexpr quint32 nativeMissionBriefingSite = 0x801dc4ec;
constexpr int missionBriefingCalls[] = {0x175c20, 0x175e5c};
constexpr int missionExitDispatch = 0x0ec6fc;
QByteArray missionExitDispatchCode(quint32 entry = 0);
QByteArray canonicalMissionExitCode(const QByteArray &image);
// Restore authenticated authored mission calls in a temporary image.
// This lets independent whole-owner validators still check all other bytes.
QByteArray canonicalMissionImage(const Rom &);
}
