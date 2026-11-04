#pragma once
#include "engine/scene_events.h"
namespace lordly {
struct EventFlagWriter {
    int row = -1, offset = -1, entry = -1, flag = -1, state = -1;
    QString title, detail, authoredId;
    // A native program switch clears a range. This is lifecycle behavior,
    // separate from an explicit Set/Clear action in an event tree.
    bool programReset = false;
};
class EventFlagLinks {
  public:
    static QVector<EventFlagWriter> writers(const Rom &, const SceneEvents &,
                                           const ScenePrograms &, int contextRow, int flag,
                                           bool includeProgramResets = true);
    static QVector<EventFlagWriter> writersInProgram(const Rom &, const SceneEvents &,
                                                    const ScenePrograms &, int row);
};
}
