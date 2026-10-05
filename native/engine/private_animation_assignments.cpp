#include "engine/project.h"
#include <QJsonArray>
namespace lordly {
namespace {
bool matches(const QJsonObject &row, int id, int flags, int action) {
    return row["classId"].toInt(-1) == id && row["bodyFlags"].toInt(-1) == flags &&
           row["actionId"].toInt(-1) == action;
}
QJsonValue exactPair(const QJsonArray &rows, int id, int flags, int action) {
    for (const auto &value : rows) {
        const auto row = value.toObject();
        if (matches(row, id, flags, action))
            return QJsonObject{{"normalSelector", row["normalSelector"]},
                               {"blockedSelector", row["blockedSelector"]}};
    }
    return QJsonValue(QJsonValue::Null);
}
QJsonArray replacePair(const QJsonArray &rows, int id, int flags, int action,
                       const QJsonValue &pair) {
    QJsonArray result;
    for (const auto &value : rows)
        if (!matches(value.toObject(), id, flags, action))
            result << value;
    if (!pair.isNull()) {
        auto row = pair.toObject();
        row["classId"] = id;
        row["bodyFlags"] = flags;
        row["actionId"] = action;
        result << row;
    }
    return result;
}
void validate(const AnimationSequences &sequences, const AnimationOverrides &overrides) {
    for (auto it = sequences.groups().begin(); it != sequences.groups().end(); ++it) {
        const auto &baselines = it->attackBaselines;
        if (baselines.isEmpty())
            continue;
        if (!overrides.available())
            throw Error("Private attacks require an editable animation override owner");
        const auto bank = it->descriptor.poses();
        for (auto action = baselines.begin(); action != baselines.end(); ++action) {
            const auto pair =
                exactPair(overrides.entries(), it.key() / 4, it.key() % 4, action.key().toInt());
            if (pair.isNull())
                throw Error("Restore the private attack before removing its override");
            for (const auto *key : {"normalSelector", "blockedSelector"}) {
                const int state = pair.toObject()[key].toInt(-1);
                if (state < 0 || state >= bank.programs().size() ||
                    bank.effective(state).records.isEmpty())
                    throw Error("A private attack selects an absent or empty pose");
                bank.validateEntry(state);
            }
        }
    }
}
} // namespace
void Project::validateAnimationAssignments() {
    loadAnimationRoutes();
    loadAnimationOverrides();
    validate(*animationSequences_, *animationOverrides_);
}
void Project::setPrivateAnimationState(const QJsonObject &sequenceData,
                                       const QJsonObject &overrideData) {
    loadAnimationRoutes();
    loadAnimationOverrides();
    auto sequences = *animationSequences_;
    auto overrides = *animationOverrides_;
    sequences.apply(sequenceData);
    sequences.validate(*animationRoutes_, records(Table::Classes).size());
    if (overrideData != overrides.json())
        overrides.apply(overrideData, rom_, classData(), *animationRoutes_);
    validate(sequences, overrides);
    for (auto it = animationSequences_->groups().begin(); it != animationSequences_->groups().end();
         ++it) {
        const auto next = sequences.groups().value(it.key()).attackBaselines;
        for (auto action = it->attackBaselines.begin(); action != it->attackBaselines.end();
             ++action)
            if (!next.contains(action.key()) &&
                exactPair(overrides.entries(), it.key() / 4, it.key() % 4, action.key().toInt()) !=
                    action.value())
                throw Error("Removing a private attack must restore its prior assignment");
    }
    animationSequences_ = std::move(sequences);
    animationOverrides_ = std::move(overrides);
}
int Project::assignPrivateAttack(int id, int flags, int action, bool blocked, int pose,
                                 bool duplicate) {
    loadAnimationRoutes();
    loadAnimationOverrides();
    const auto *original = animationSequences_->find(id, flags);
    if (!original)
        throw Error("Make a private copy before assigning an attack");
    if (!animationOverrides_->available())
        throw Error(animationOverrides_->unavailableReason());
    auto descriptor = *original;
    auto bank = descriptor.poses();
    const auto source = bank.effective(pose).source;
    if (bank.effective(pose).records.isEmpty())
        throw Error("Choose a pose with instructions");
    const auto classes = classData();
    const int ownerByte = u8(classes.entries()[id].record, 69), owner = ownerByte ? ownerByte : id;
    int selected = pose;
    if (duplicate) {
        if (bank.programs().size() >= 256)
            throw Error("The attack selector's one-byte range is full");
        while (bank.programs().size() < 100)
            bank.appendProgram(QByteArray(1, 0));
        selected = bank.appendProgram(source);
        auto records = bank.effective(selected).records;
        for (auto &record : records)
            if (record.opcode == 5 && u8(record.operands, 0) == pose)
                record.operands[0] = char(selected);
        bank.setProgram(selected, PoseBank::encodeProgram(records));
    }
    if (selected < 0 || selected > 255)
        throw Error("Attack assignments require a one-byte selector");
    if ((owner == 10 || owner == 25 || owner == 123) && selected >= 50)
        throw Error("This linked body needs extended secondary-pose routing before assigning this "
                    "selector");
    if ((id == 135 || id == 136 || id == 161) && selected != 0 && selected != 50)
        throw Error("This class needs its fixed-pose runtime policy rebuilt before assigning this "
                    "selector");
    descriptor.setPoses(bank);
    auto sequences = *animationSequences_;
    sequences.setDescriptor(id, flags, descriptor);
    sequences.rememberAttackBaseline(id, flags, action,
                                     exactPair(animationOverrides_->entries(), id, flags, action));
    const auto current = bodyAnimationData();
    QJsonObject pair{{"normalSelector", current->selector(id, owner, action, 0, flags)},
                     {"blockedSelector", current->selector(id, owner, action, 2, flags)}};
    pair[blocked ? "blockedSelector" : "normalSelector"] = selected;
    auto overrides = *animationOverrides_;
    overrides.setEntries(replacePair(overrides.entries(), id, flags, action, pair), rom_, classes,
                         *animationRoutes_);
    setPrivateAnimationState(sequences.json(), overrides.json());
    return selected;
}
void Project::restorePrivateAttack(int id, int flags, int action) {
    loadAnimationRoutes();
    loadAnimationOverrides();
    auto sequences = *animationSequences_;
    const auto baselines = sequences.groups().value(id * 4 + flags).attackBaselines;
    const auto key = QString::number(action);
    if (!baselines.contains(key))
        return;
    auto overrides = *animationOverrides_;
    overrides.setEntries(replacePair(overrides.entries(), id, flags, action, baselines[key]), rom_,
                         classData(), *animationRoutes_);
    sequences.clearAttackBaseline(id, flags, action);
    setPrivateAnimationState(sequences.json(), overrides.json());
}
void Project::removePrivateAnimation(int id, int flags) {
    loadAnimationRoutes();
    loadAnimationOverrides();
    auto sequences = *animationSequences_;
    auto overrides = *animationOverrides_;
    auto rows = overrides.entries();
    const auto baselines = sequences.groups().value(id * 4 + flags).attackBaselines;
    for (auto action = baselines.begin(); action != baselines.end(); ++action)
        rows = replacePair(rows, id, flags, action.key().toInt(), action.value());
    if (!baselines.isEmpty())
        overrides.setEntries(rows, rom_, classData(), *animationRoutes_);
    sequences.remove(id, flags);
    setPrivateAnimationState(sequences.json(), overrides.json());
}
} // namespace lordly
