#pragma once
#include "engine/scene_events.h"
#include "engine/story_routes.h"

namespace lordly {
struct EntryFlagCondition {
    int flag = -1, value = 0, minimum = 0, maximum = 255;
    QVector<int> flagOffsets, comparisonOffsets;
    QMap<int, int> sourceWords;
    bool stateEditable = true;
    QString unavailable;
    // Byte parts of the literal token; preserved separately from its meaning.
    QMap<int, int> literalShifts;
    QMap<int, bool> literalInversions;
};
struct EntryNumberCondition {
    QString name = "Chaos Frame";
    int value = 0, relation = 10, minimum = 0, maximum = 100;
    int valueOffset = -1, comparisonOffset = -1;
    bool reversedOperands = false, branchTaken = false;
    QMap<int, int> sourceWords;
};
struct EntryConditions {
    QVector<EntryFlagCondition> flags;
    QVector<EntryNumberCondition> numbers;
    int event = -1, eventOffset = -1;
    QMap<int, int> sourceWords;
    static EntryConditions read(const Rom &, const SceneEvents &, const ScenePrograms &,
                                const StoryRoute &, int chaos);
    static void setFlag(SceneEvents &, const ScenePrograms &, const EntryFlagCondition &, int flag);
    static void setState(SceneEvents &, const ScenePrograms &, const EntryFlagCondition &, int state);
    static void setNumberValue(SceneEvents &, const ScenePrograms &, const EntryNumberCondition &, int value);
    static void setNumberRelation(SceneEvents &, const ScenePrograms &, const EntryNumberCondition &, int relation);
    static void setEvent(SceneEvents &, const ScenePrograms &, const EntryConditions &, int event);
};
} // namespace lordly
