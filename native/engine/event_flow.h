#pragma once
#include "engine/event_script.h"
#include "engine/scene_events.h"
#include <array>
namespace lordly {
class CampaignMap;
class Dialogues;
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
    int answerScene = -1;
    int requestOffset = -1; // Event opcode that launches scene, including in a folded visit.
};
struct PlayerAnswer {
    int value = -1, destination = -1;
    QString text;
    QStringList effects;
};
struct PlayerDecision {
    int entry = -1, group = -1, archive = -1, dialogue = -1, selector = -1, questionOffset = -1;
    QString scene;
    QStringList commonEffects;
    StoryField answerField;
    std::array<PlayerAnswer,2> answers;
};
struct SceneAnswerReaction {
    int selector = -1, requestOffset = -1;
    DirectorLaunchTranslations translations;
    QStringList effects;
};
struct SceneAnswerDecision {
    int selector = -1, comparedAnswer = -1;
    QStringList commonEffects;
    QMap<int,bool> commonFlags;
    SceneAnswerReaction equal, different;
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
    int ruleIndex = -1; // Zero-based position within this trigger's native table.
};
class EventFlow {
  public:
    // Projection of current instructions, never a second saved event model.
    static QVector<StoryEvent> read(const Rom &, const SceneEvents &, const ScenePrograms &, int row,
                                    bool originalSceneText = true,bool structural=true,const CampaignMap *locations=nullptr,
                                    const Dialogues *dialogues=nullptr);
    static void set(SceneEvents &, const ScenePrograms &, int row, const StoryField &, int value);
    static QVector<PlayerDecision> playerDecisions(const QVector<StoryEvent> &, const QByteArray &program,
        const ScenePrograms &, const Dialogues &, const QJsonObject &gates);
    static std::optional<SceneAnswerDecision> sceneDecision(const QVector<StoryEvent> &, const QByteArray &program,
        const SceneEventSite &, int selector, const QJsonObject &gates);
    // The order here is the order shown in the dialogue, independent of the byte values.
    static QVector<QPair<int,QString>> questionAnswers(int selector,const ScenePrograms &,const Dialogues &);
    static QVector<int> questionSelectors(const ScenePrograms &);
};
}
