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
void validate(const AnimationSequences &sequences, const AnimationOverrides &overrides,
              const ClassData &classes) {
    for (auto it = sequences.groups().begin(); it != sequences.groups().end(); ++it) {
        const auto &baselines = it->attackBaselines;
        if (baselines.isEmpty())
            continue;
        if (!overrides.available())
            throw Error("Private attacks require an editable animation override owner");
        const auto bank = it->descriptor.poses();
        const int id = it.key() / 4, ownerByte = u8(classes.entries()[id].record, 69),
                  owner = ownerByte ? ownerByte : id;
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
                if ((owner == 10 || owner == 25 || owner == 123) && state >= 50 &&
                    !it->pairedPoses.contains(state))
                    throw Error("A linked private attack needs its secondary pose assignment");
            }
        }
    }
}
} // namespace
void Project::validateAnimationAssignments() {
    loadAnimationRoutes();
    loadAnimationOverrides();
    validate(*animationSequences_, *animationOverrides_, classData());
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
    validate(sequences, overrides, classData());
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
    const bool linked = owner == 10 || owner == 25 || owner == 123;
    auto group = animationSequences_->groups().value(id * 4 + flags);
    int selected = pose;
    if (duplicate) {
        int companion = -1;
        if (linked) {
            companion = pose < 50 ? pose + 50 : group.pairedPoses.value(pose, -1);
            if (companion < 0 || companion >= bank.programs().size() ||
                bank.effective(companion).records.isEmpty())
                throw Error("Choose a primary pose with a linked secondary program");
            bank.validateEntry(companion);
            for (const auto &record : bank.effective(companion).records)
                if (record.opcode == 5 && u8(record.operands, 0) == companion)
                    throw Error("This secondary program needs a wide self-transfer before it can be copied");
        }
        if (group.reservedAttackStart) {
            selected = group.reservedAttackStart;
            while (selected < 256 && (group.pairedPoses.contains(selected) ||
                                      bank.programs()[selected].source != QByteArray(1, 0)))
                ++selected;
            if (selected == 256)
                throw Error("The attack selector's one-byte range is full");
            bank.setProgram(selected, source);
        } else {
            if (bank.programs().size() >= 256)
                throw Error("The attack selector's one-byte range is full");
            while (bank.programs().size() < 100)
                bank.appendProgram(QByteArray(1, 0));
            selected = bank.appendProgram(source);
        }
        auto records = bank.effective(selected).records;
        for (auto &record : records)
            if (record.opcode == 5 && u8(record.operands, 0) == pose)
                record.operands[0] = char(selected);
        bank.setProgram(selected, PoseBank::encodeProgram(records));
        if (linked) {
            const auto secondary = bank.effective(companion).source;
            if (!group.reservedAttackStart)
                group.reservedAttackStart = selected + 1;
            while (bank.programs().size() < 256)
                bank.appendProgram(QByteArray(1, 0));
            // State-transfer operands are one byte. Keep their original targets;
            // the copied companion itself has an independent signed-halfword state.
            group.pairedPoses[selected] = bank.appendProgram(secondary);
        }
    }
    if (selected < 0 || selected > 255)
        throw Error("Attack assignments require a one-byte selector");
    if (linked && selected >= 50 && !group.pairedPoses.contains(selected))
        throw Error("This private attack has no linked secondary program");
    if ((id == 135 || id == 136 || id == 161) && selected != 0 && selected != 50)
        throw Error("This class needs its fixed-pose runtime policy rebuilt before assigning this "
                    "selector");
    descriptor.setPoses(bank);
    auto sequences = *animationSequences_;
    sequences.setDescriptor(id, flags, descriptor);
    sequences.setPairedPoses(id, flags, group.pairedPoses, group.reservedAttackStart);
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
QByteArray Project::privatePoseMappings() {
    validateAnimationAssignments();
    QMap<quint32, quint16> mappings;
    const auto classes = classData();
    for (auto it = animationSequences_->groups().begin(); it != animationSequences_->groups().end();
         ++it) {
        const int id = it.key() / 4, flags = it.key() % 4;
        const int ownerByte = u8(classes.entries()[id].record, 69),
                  owner = ownerByte ? ownerByte : id;
        for (auto action = it->attackBaselines.begin(); action != it->attackBaselines.end();
             ++action) {
            const auto pair =
                exactPair(animationOverrides_->entries(), id, flags, action.key().toInt())
                    .toObject();
            for (const auto *lane : {"normalSelector", "blockedSelector"}) {
                const int primary = pair[lane].toInt();
                if (!it->pairedPoses.contains(primary))
                    continue;
                const quint32 key = (quint32(id) << 24) | (quint32(owner) << 16) |
                                    (quint32(flags) << 8) | quint32(primary);
                mappings[key] = quint16(it->pairedPoses.value(primary));
            }
        }
    }
    QByteArray bytes;
    for (auto it = mappings.begin(); it != mappings.end(); ++it) {
        QByteArray row(8, 0);
        put32(row, 0, it.key());
        put16(row, 4, it.value());
        bytes += row;
    }
    auto routes = *animationRoutes_;
    routes.setPrivatePoses(bytes); // One shared serialization/domain check.
    return bytes;
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
