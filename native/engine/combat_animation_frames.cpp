#include "engine/combat_animation_frames.h"
#include "engine/sprite_colors.h"
#include "engine/actor_art.h"
#include "engine/animation_transfer.h"
#include "engine/sprite_object.h"
#include <QSet>
#include <algorithm>
namespace lordly {
namespace {
bool isFrame(const PoseRecord &record) {
    return record.opcode == 1 || record.opcode == 21;
}
int owner(Project &project, int id) {
    const int shared = u8(project.record(Table::Classes, id).bytes, 69);
    return shared ? shared : id;
}
ActorArt body(Project &project, int id, int flags) {
    if (flags < 0 || flags > 3)
        throw Error("Invalid animation artwork variant");
    return ActorArt::loadBody(project.rom(), id, owner(project, id), flags >> 1, flags & 1,
                              project.bodyAnimationData());
}
void prepare(Project &project) {
    project.loadAnimationRoutes();
    project.loadAnimationOverrides();
    project.loadCombatArtwork();
}
AnimationSequences editable(Project &project, CombatAnimationFrameTarget target) {
    auto sequences = *project.animationSequences();
    if (!sequences.find(target.classId, target.flags))
        sequences.create(target.classId, target.flags, *project.animationRoutes(),
                         project.combatArtwork()->previewEdits());
    return sequences;
}
int insertionPosition(const AnimationDescriptor &descriptor, CombatAnimationFrameTarget target) {
    if (target.record < -1)
        throw Error("Invalid destination animation frame");
    const auto bank = descriptor.poses();
    if (target.pose < 0 || target.pose >= bank.programs().size())
        throw Error("Create this animation before adding frames");
    const auto records = bank.effective(target.pose).records;
    if (target.replace || target.record >= 0) {
        if (target.record < 0 || target.record >= records.size() ||
            !isFrame(records[target.record]))
            throw Error("Choose a destination animation frame");
        return target.record + (target.replace ? 0 : 1);
    }
    // Keep the original trailing controls after appended artwork. Empty native
    // programs insert at their beginning, before an END or a dependent-program jump.
    for (int i = int(records.size()) - 1; i >= 0; --i)
        if (isFrame(records[i]))
            return i + 1;
    return 0;
}
void validateSelection(const QVector<int> &selection, int count, bool replace) {
    if (selection.isEmpty() || (replace && selection.size() != 1))
        throw Error(replace ? "Choose exactly one replacement frame" : "Choose frames to add");
    QSet<int> seen;
    for (const int index : selection) {
        if (index < 0 || index >= count || seen.contains(index))
            throw Error("Invalid animation frame selection");
        seen.insert(index);
    }
}
void checkCapacity(const AnimationDescriptor &descriptor, CombatAnimationFrameTarget target,
                   int added) {
    if (!target.replace && descriptor.poses().effective(target.pose).records.size() + added > 255)
        throw Error("These frames exceed the animation's 255-instruction limit. Import or copy the "
                    "whole animation instead.");
}
Project finish(Project project, AnimationSequences sequences, CombatAnimationFrameTarget target,
               const AnimationDescriptor &descriptor) {
    sequences.setDescriptor(target.classId, target.flags, descriptor);
    project.setAnimationSequences(sequences.json());
    project.validateAnimationAssignments();
    return project;
}
} // namespace
Project importCombatAnimationFrames(Project project, CombatAnimationFrameTarget target,
                                    const SpriteAsset &asset, const QVector<int> &sourceFrames,
                                    bool dither) {
    asset.validate();
    validateSelection(sourceFrames, int(asset.frames.size()), target.replace);
    if (asset.anchor.x() == -32768 || asset.anchor.y() == -32768)
        throw Error("The artwork anchor exceeds signed game coordinates");
    prepare(project);
    auto sequences = editable(project, target);
    auto descriptor = *sequences.find(target.classId, target.flags);
    int position = insertionPosition(descriptor, target);
    int holds = 0;
    for (const int frame : sourceFrames)
        holds += target.replace ? 1 : (std::max(1, asset.frames[frame].ticks) * 2 + 253) / 254;
    checkCapacity(descriptor, target, holds);
    auto bank = descriptor.poses();
    const int retainedTicks =
        target.replace ? u8(bank.effective(target.pose).records[target.record].operands,
                            bank.effective(target.pose).records[target.record].operands.size() - 1)
                       : 0;
    const auto normalized = spriteToSrgb(asset);
    for (const int frame : sourceFrames) {
        const auto rgba = normalized.composite(frame);
        const auto sprite = SpriteObject::fromImage(
            {rgba.width(), rgba.height(),
             QByteArray(reinterpret_cast<const char *>(rgba.constBits()), rgba.sizeInBytes())},
            dither);
        const int art =
            descriptor.appendSprite(sprite.source(), 0, descriptor.controls()[3].left(512));
        QByteArray row(18, 0);
        put16(row, 0, 1);
        put16(row, 2, quint16(art));
        put16(row, 4, quint16(-asset.anchor.x()));
        put16(row, 6, quint16(-asset.anchor.y()));
        put16(row, 8, quint16(rgba.width()));
        put16(row, 10, quint16(rgba.height()));
        put16(row, 14, 1024);
        put16(row, 16, 1024);
        const int token = descriptor.appendFrame(row);
        if (target.replace)
            bank.setFrame(target.pose, target.record, token, retainedTicks);
        else {
            int ticks = std::max(1, asset.frames[frame].ticks) * 2;
            while (ticks) {
                const int hold = std::min(ticks, 254);
                bank.insertFrame(target.pose, position++, token, hold);
                ticks -= hold;
            }
        }
    }
    descriptor.setPoses(bank);
    return finish(std::move(project), std::move(sequences), target, descriptor);
}
Project copyCombatAnimationFrames(Project project, CombatAnimationFrameTarget target,
                                  int sourceClass, int sourceFlags, int sourcePose,
                                  const QVector<int> &sourceRecords) {
    prepare(project);
    const auto art = body(project, sourceClass, sourceFlags);
    const auto sourceData = project.animationSequences()->find(sourceClass, sourceFlags);
    const auto source =
        sourceData ? *sourceData
                   : AnimationDescriptor::load(
                         project.rom(), art.descriptorKey(),
                         project.combatArtwork()->previewEdits().value(art.descriptorKey()));
    const auto records = source.poses().effective(sourcePose).records;
    validateSelection(sourceRecords, int(records.size()), target.replace);
    for (const int row : sourceRecords)
        if (!isFrame(records[row]))
            throw Error("Choose source animation frames");
    auto sequences = editable(project, target);
    auto descriptor = *sequences.find(target.classId, target.flags);
    int position = insertionPosition(descriptor, target);
    checkCapacity(descriptor, target, int(sourceRecords.size()));
    for (const int sourceRecord : sourceRecords) {
        if (!target.replace) {
            auto bank = descriptor.poses();
            // copyAnimationFrame replaces one frame while preserving native
            // layers, palettes and equipment policies. Insert a temporary valid
            // token first; the final candidate never publishes that placeholder.
            bank.insertFrame(target.pose, position, 0, 1);
            descriptor.setPoses(bank);
        }
        descriptor = copyAnimationFrame(project.rom(), descriptor, target.pose, position, source,
                                        art, sourcePose, sourceRecord, !target.replace);
        ++position;
    }
    return finish(std::move(project), std::move(sequences), target, descriptor);
}
} // namespace lordly
