#include "engine/combat_animation_edit.h"
#include "engine/actor_art.h"
#include "engine/animation_transfer.h"
#include <QJsonArray>
#include <QSet>
#include <algorithm>
namespace lordly {
QMap<int, int> combatMotions() {
    return {{-1, 0}, {-2, 5}, {-3, 11}, {-4, 17}};
}
QMap<int, QString> combatMotionNames() {
    return {{-1, "Idle"}, {-2, "Advance"}, {-3, "Return to formation"}, {-4, "Hit / recoil"}};
}
namespace {
int owner(Project &p, int id) {
    const int n = u8(p.record(Table::Classes, id).bytes, 69);
    return n ? n : id;
}
bool linked(Project &p, int id) {
    const int n = owner(p, id);
    return n == 10 || n == 25 || n == 123;
}
ActorArt art(Project &p, CombatAnimationTarget t) {
    return ActorArt::loadBody(p.rom(), t.classId, owner(p, t.classId), t.flags >> 1, t.flags & 1,
                              p.bodyAnimationData());
}
AnimationDescriptor descriptor(Project &p, CombatAnimationTarget t) {
    const auto privateData = p.animationSequences()->find(t.classId, t.flags);
    return privateData ? *privateData
                       : AnimationDescriptor::load(
                             p.rom(), art(p, t).descriptorKey(),
                             p.combatArtwork()->previewEdits().value(art(p, t).descriptorKey()));
}
QMap<int, QString> actionNames() {
    QMap<int, QString> names;
    for (const auto &v : AnimationOverrideImage::definition()["actions"].toArray()) {
        const auto r = v.toObject();
        names[r["id"].toInt()] = r["name"].toString();
    }
    return names;
}
int companion(Project &p, CombatAnimationTarget t, int pose) {
    if (!linked(p, t.classId))
        return -1;
    return pose < 50 ? pose + 50
                     : p.animationSequences()
                           ->groups()
                           .value(t.classId * 4 + t.flags)
                           .pairedPoses.value(pose, -1);
}
int prepare(Project &p, CombatAnimationTarget t) {
    p.loadCombatArtwork();
    p.loadAnimationRoutes();
    p.loadAnimationOverrides();
    auto sequences = *p.animationSequences();
    if (!sequences.find(t.classId, t.flags))
        sequences.create(t.classId, t.flags, *p.animationRoutes(),
                         p.combatArtwork()->previewEdits());
    p.setAnimationSequences(sequences.json());
    int pose = combatAnimationPose(p, t);
    if (t.action >= 0) {
        const auto bank = sequences.find(t.classId, t.flags)->poses();
        if (pose < 0 || pose >= bank.programs().size() || bank.effective(pose).records.isEmpty())
            pose = 0;
        pose = p.assignPrivateAttack(t.classId, t.flags, t.action, t.blocked, pose, true);
    }
    // Keep one-byte attack selectors available when a copied sequence needs
    // additional programs. Dependencies live beyond that reserved area.
    sequences = *p.animationSequences();
    const auto group = sequences.groups().value(t.classId * 4 + t.flags);
    auto d = *sequences.find(t.classId, t.flags);
    auto bank = d.poses();
    int start = group.reservedAttackStart;
    if (!start && bank.programs().size() < 256) {
        start = std::max(101, int(bank.programs().size()));
        while (bank.programs().size() < 256)
            bank.appendProgram(QByteArray(1, 0));
        d.setPoses(bank);
        sequences.setDescriptor(t.classId, t.flags, d);
        sequences.setPairedPoses(t.classId, t.flags, group.pairedPoses, start);
        p.setAnimationSequences(sequences.json());
    }
    return pose;
}
void finish(Project &p, CombatAnimationTarget t, AnimationDescriptor d, int pose) {
    if (t.action == -1) {
        auto bank = d.poses();
        for (int idle : {1, 2, 3, 4, 8})
            bank.setProgram(idle, bank.effective(pose).source);
        if (linked(p, t.classId))
            for (int idle : {51, 52, 53, 54, 58})
                bank.setProgram(idle, bank.effective(50).source);
        d.setPoses(bank);
    }
    auto sequences = *p.animationSequences();
    sequences.setDescriptor(t.classId, t.flags, d);
    p.setAnimationSequences(sequences.json());
    p.validateAnimationAssignments();
}
void framePrograms(PoseBank &bank, int pose, const QVector<QPair<int, int>> &frames, bool loop) {
    QVector<int> states{pose};
    const int chunks = (frames.size() + 252) / 253;
    for (int n = 1; n < chunks; ++n)
        states << bank.appendProgram(QByteArray(1, 0));
    // Write tail first so forward references never select an empty placeholder.
    for (int n = chunks - 1; n >= 0; --n) {
        QVector<PoseRecord> records;
        for (int i = n * 253; i < std::min(int(frames.size()), (n + 1) * 253); ++i) {
            const auto [frame, ticks] = frames[i];
            records << PoseRecord{21, QByteArray(1, char(frame)) + QByteArray(1, char(frame >> 8)) +
                                          QByteArray(1, char(ticks))};
        }
        const int next = n + 1 < states.size() ? states[n + 1] : loop ? pose : -1;
        if (next >= 0)
            records << PoseRecord{22, QByteArray(1, char(next)) + QByteArray(1, char(next >> 8))};
        else
            records << PoseRecord{0, {}};
        bank.setProgram(states[n], PoseBank::encodeProgram(records));
    }
}
int transparentFrame(AnimationDescriptor &d) {
    QImage image(1, 1, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    const auto sprite = SpriteObject::fromImage(
        {1, 1, QByteArray(reinterpret_cast<const char *>(image.constBits()), 4)});
    const int art = d.appendSprite(sprite.source(), 0, d.controls()[3].left(512));
    QByteArray row(18, 0);
    put16(row, 0, 1);
    put16(row, 2, quint16(art));
    put16(row, 8, 1);
    put16(row, 10, 1);
    put16(row, 14, 1024);
    put16(row, 16, 1024);
    return d.appendFrame(row);
}
} // namespace
int combatAnimationPose(Project &p, CombatAnimationTarget t) {
    if (t.action < 0) {
        if (!combatMotions().contains(t.action))
            throw Error("Unknown combat animation");
        return combatMotions()[t.action];
    }
    return p.bodyAnimationData()->selector(t.classId, owner(p, t.classId), t.action,
                                           t.blocked ? 2 : 0, t.flags);
}
QString combatAnimationName(Project &, CombatAnimationTarget t) {
    return t.action < 0 ? combatMotionNames().value(t.action)
                        : actionNames().value(t.action, QString("Action %1").arg(t.action)) +
                              (t.blocked ? " — blocked" : "");
}
QVector<NamedCombatAnimation> namedCombatAnimations(Project &p, int id, int flags) {
    p.loadAnimationRoutes();
    p.loadAnimationOverrides();
    p.loadCombatArtwork();
    QVector<NamedCombatAnimation> out;
    const CombatAnimationTarget target{id, flags};
    const auto bank = descriptor(p, target).poses();
    const auto add = [&](int action, bool blocked) {
        const CombatAnimationTarget t{id, flags, action, blocked};
        const int pose = combatAnimationPose(p, t);
        if (pose < 0 || pose >= bank.programs().size() || pose == 38 || pose == 88)
            return;
        try {
            bank.validateEntry(pose);
            if (bank.effective(pose).records.isEmpty())
                return;
        } catch (const Error &) {
            return;
        }
        out << NamedCombatAnimation{combatAnimationName(p, t), action, pose, blocked};
    };
    for (int motion : {-1, -2, -3, -4})
        add(motion, false);
    QSet<int> actions;
    const auto current = p.record(Table::Classes, id).bytes,
               base = p.record(Table::Classes, owner(p, id)).bytes;
    for (int n = 0; n < 3; ++n) {
        int action = u8(current, 55 + n * 2);
        if (action == 255)
            action = u8(base, 55 + n * 2);
        if (action != 255)
            actions.insert(action);
    }
    auto selectors = p.animationRoutes()->entry(id).selectors;
    if (u8(current, 69) != owner(p, id) || selectors.isEmpty())
        selectors = p.animationRoutes()->entry(owner(p, id)).selectors;
    for (int at = 0; at + 3 < selectors.size(); at += 4)
        actions.insert(u8(selectors, at));
    auto sorted = actions.values();
    std::sort(sorted.begin(), sorted.end());
    for (int action : sorted) {
        add(action, false);
        add(action, true);
    }
    return out;
}
Project replaceCombatAnimation(Project p, CombatAnimationTarget t, const SpriteAsset &asset,
                               bool loop, bool dither) {
    asset.validate();
    if (asset.anchor.x() == -32768 || asset.anchor.y() == -32768)
        throw Error("Animation anchor cannot be represented in signed game coordinates");
    const int pose = prepare(p, t), secondary = companion(p, t, pose);
    if (linked(p, t.classId) && secondary < 0)
        throw Error("The destination is missing its linked body animation");
    auto d = *p.animationSequences()->find(t.classId, t.flags);
    QVector<QPair<int, int>> frames;
    for (int i = 0; i < asset.frames.size(); ++i) {
        const auto rgba = asset.composite(i).convertToFormat(QImage::Format_RGBA8888);
        const auto sprite = SpriteObject::fromImage(
            {rgba.width(), rgba.height(),
             QByteArray(reinterpret_cast<const char *>(rgba.constBits()), rgba.sizeInBytes())},
            dither);
        const int index = d.appendSprite(sprite.source(), 0, d.controls()[3].left(512));
        QByteArray row(18, 0);
        put16(row, 0, 1);
        put16(row, 2, quint16(index));
        put16(row, 4, quint16(-asset.anchor.x()));
        put16(row, 6, quint16(-asset.anchor.y()));
        put16(row, 8, quint16(rgba.width()));
        put16(row, 10, quint16(rgba.height()));
        put16(row, 14, 1024);
        put16(row, 16, 1024);
        const int token = d.appendFrame(row);
        int ticks = std::max(1, asset.frames[i].ticks) * 2;
        while (ticks) {
            const int hold = std::min(ticks, 254);
            frames << qMakePair(token, hold);
            ticks -= hold;
        }
    }
    auto bank = d.poses();
    framePrograms(bank, pose, frames, loop);
    if (secondary >= 0) {
        const int empty = transparentFrame(d);
        auto holds = frames;
        for (auto &hold : holds)
            hold.first = empty;
        framePrograms(bank, secondary, holds, loop);
    }
    d.setPoses(bank);
    finish(p, t, d, pose);
    return p;
}
Project copyCombatAnimation(Project p, CombatAnimationTarget t, CombatAnimationTarget source) {
    p.loadAnimationRoutes();
    p.loadAnimationOverrides();
    p.loadCombatArtwork();
    const auto sourceArt = art(p, source);
    const auto sourceData = descriptor(p, source);
    const int sourcePose = combatAnimationPose(p, source),
              sourceCompanion = companion(p, source, sourcePose);
    const int pose = prepare(p, t), secondary = companion(p, t, pose);
    auto d = copyAnimationPose(p.rom(), *p.animationSequences()->find(t.classId, t.flags), pose,
                               sourceData, sourceArt, sourcePose, true);
    if (linked(p, t.classId)) {
        if (secondary < 0)
            throw Error("The destination is missing its linked body animation");
        if (sourceCompanion >= 0)
            d = copyAnimationPose(p.rom(), d, secondary, sourceData, sourceArt, sourceCompanion,
                                  true);
        else {
            auto bank = d.poses();
            const int empty = transparentFrame(d);
            framePrograms(bank, secondary, {{empty, 2}}, true);
            d.setPoses(bank);
        }
    } else if (sourceCompanion >= 0) {
        throw Error("This source has two linked body parts. Choose a class with a matching body "
                    "setup, or import frame artwork files.");
    }
    finish(p, t, d, pose);
    return p;
}
} // namespace lordly
