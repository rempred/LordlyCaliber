#pragma once
#include "engine/event_script.h"
#include "engine/scene_events.h"
namespace lordly {
class CampaignMap;
struct StoryField {
    QString label, key;
    int offset = -1, value = 0, minimum = 0, maximum = 255;
    QMap<int, QString> choices;
    QString kind;
    enum Mode { Operand, FlagValue, WorldRule, SquadState, BooleanComparison, VisitorCheck } mode = Operand;
    int mask = 65535;
    int location = -1, rule = -1, condition = -1, shift = 0;
};
struct StoryCondition {
    // test = atomic predicate; all/any/not = proven boolean composition.
    // Predicate editors belong here, not also in StoryAction::fields.
    QString operation, title;
    QVector<StoryCondition> children;
    QVector<StoryField> fields;
};
struct StoryAction {
    QString kind, title, detail;
    QVector<int> instructions;
    QVector<StoryField> fields;
    int target = -1, otherwise = -1, scene = -1;
    bool advanced = false;
    StoryCondition condition;
};
struct StoryEvent {
    int entry = 0, group = -1;
    QString title, trigger;
    QVector<StoryField> triggerFields;
    QVector<int> triggerInstructions;
    QVector<StoryAction> requirements;
    QVector<StoryAction> actions;
    bool simpleVisit = false;
    QString authoredId;
};
class EventFlow {
  public:
    // Projection of current instructions, never a second saved event model.
    static QVector<StoryEvent> read(const Rom &, const SceneEvents &, const ScenePrograms &, int row,
                                    bool originalSceneText = true,bool structural=true,const CampaignMap *locations=nullptr);
    static void set(SceneEvents &, const ScenePrograms &, int row, const StoryField &, int value);
};
}
