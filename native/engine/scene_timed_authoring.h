#pragma once
#include "engine/scene_actions.h"
#include "engine/scene_camera.h"
#include <QMap>
#include <QSet>
namespace lordly {
struct TimedSceneEdit {
    DirectorProgram program;
    int command = -1;
};
TimedSceneEdit insertSceneActionAtTime(const DirectorProgram &, const QMap<int, int> &commandTimes,
                                      int tick, quint32 kind, int duration = 30);
TimedSceneEdit placeSceneActorAtTime(const DirectorProgram &, const QMap<int, int> &commandTimes,
                                    int tick, int slot, SceneVector position);
DirectorProgram holdSceneBeforeExit(const DirectorProgram &, int exitCommand,
                                    const QSet<int> &movingActors, bool fade, int panUpdates);
} // namespace lordly
