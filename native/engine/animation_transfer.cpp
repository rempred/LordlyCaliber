#include "engine/animation_transfer.h"
namespace lordly {
namespace {
int cloneFrame(const Rom &rom, AnimationDescriptor &target, const AnimationDescriptor &source,
               const ActorArt &art, int token, QMap<int, int> &sprites, QMap<int, int> &frames) {
    const auto &config = source.controls()[2];
    const auto appearance = art.defaultAppearance(rom);
    if (frames.contains(token)) return frames[token];
    const auto layers = art.bodyLayers(rom, token, appearance);
    QByteArray row(2 + layers.size() * 16, 0);
    put16(row, 0, quint16(layers.size()));
    for (int i = 0; i < layers.size(); ++i) {
        const auto &layer = layers[i].metadata;
        if (!sprites.contains(layer.art)) {
            auto bytes = source.sprite(layer.art);
            const auto sprite = SpriteObject::decode(bytes);
            int policy = u8(config, 32 + layer.art);
            if (policy == 2 && art.bodyAnimations()->routes().entry(art.sourceClass()).equipmentChildArt)
                policy = 1;
            if (policy == 0) {
                const int child = layers[i].child < sprite.children().size() ? layers[i].child : 0;
                const auto lanes = sprite.materialize(child);
                const auto header = sprite.children()[child];
                bytes = bytes.left(8);
                bytes[2] = 1;
                QByteArray next(8, 0);
                put16(next, 0, 0x5554);
                put8(next, 3, quint8((lanes.color.isEmpty() ? 0 : 1) | (lanes.alpha.isEmpty() ? 0 : 2) |
                                    (lanes.palette.isEmpty() ? 0 : 4)));
                put16(next, 4, quint16(header.widthField)); put16(next, 6, quint16(header.heightField));
                bytes += next + lanes.color + lanes.alpha + lanes.palette;
            }
            const auto palette = source.controls()[3].mid(u8(config, u32(config, 28) + layer.art) * 512, 512);
            sprites[layer.art] = target.appendSprite(bytes, policy, palette);
        }
        const int at = 2 + i * 16;
        put16(row, at, quint16(sprites[layer.art]));
        put16(row, at + 2, quint16(layer.origin.x())); put16(row, at + 4, quint16(layer.origin.y()));
        put16(row, at + 6, quint16(layer.width)); put16(row, at + 8, quint16(layer.height));
        put16(row, at + 10, layer.flags); put16(row, at + 12, layer.scaleX); put16(row, at + 14, layer.scaleY);
    }
    return frames[token] = target.appendFrame(row);
}
} // namespace
AnimationDescriptor copyAnimationPose(const Rom &rom, AnimationDescriptor target, int targetPose,
                                      const AnimationDescriptor &source, const ActorArt &art, int sourcePose,
                                      bool copyControls) {
    const auto sourceBank = source.poses();
    auto bank = target.poses();
    bank.effective(targetPose);
    sourceBank.validateEntry(sourcePose);
    QMap<int, int> sprites, frames, states{{sourcePose, targetPose}};
    QVector<int> pending{sourcePose};
    for (int p = 0; p < pending.size(); ++p) {
        const int state = pending[p];
        auto records = sourceBank.effective(state).records;
        QVector<QPair<int, int>> timing;
        for (auto &record : records) {
            if (record.opcode == 1 || record.opcode == 21) {
                const int token = u8(record.operands, 0) + (record.opcode == 21 ? 256 * u8(record.operands, 1) : 0);
                const int replacement = cloneFrame(rom, target, source, art, token, sprites, frames), ticks = u8(record.operands, record.operands.size() - 1);
                record = {21, QByteArray(1, char(replacement)) + QByteArray(1, char(replacement >> 8)) + QByteArray(1, char(ticks))};
                timing << qMakePair(replacement, ticks);
            } else if (copyControls && (record.opcode == 5 || record.opcode == 22)) {
                const int dependency = u8(record.operands, 0) + (record.opcode == 22 ? 256 * u8(record.operands, 1) : 0);
                sourceBank.effective(dependency);
                if (!states.contains(dependency)) {
                    states[dependency] = bank.appendProgram(QByteArray(1, 0));
                    pending << dependency;
                }
                const int replacement = states[dependency];
                record = {22, QByteArray(1, char(replacement)) + QByteArray(1, char(replacement >> 8))};
            }
        }
        if (copyControls) bank.setProgram(states[state], PoseBank::encodeProgram(records));
        else bank.replaceFrames(targetPose, timing);
    }
    target.setPoses(bank);
    return target;
}
AnimationDescriptor copyAnimationFrame(const Rom &rom, AnimationDescriptor target, int targetPose, int targetRecord,
                                       const AnimationDescriptor &source, const ActorArt &art, int sourcePose,
                                       int sourceRecord, bool duration) {
    auto bank = target.poses();
    const auto destination = bank.effective(targetPose).records;
    const auto origin = source.poses().effective(sourcePose).records;
    const auto isFrame = [](const QVector<PoseRecord> &records, int row) {
        return row >= 0 && row < records.size() && (records[row].opcode == 1 || records[row].opcode == 21);
    };
    if (!isFrame(destination, targetRecord) || !isFrame(origin, sourceRecord)) throw Error("Choose source and destination frame instructions");
    const auto &record = origin[sourceRecord];
    const int token = u8(record.operands, 0) + (record.opcode == 21 ? 256 * u8(record.operands, 1) : 0);
    QMap<int, int> sprites, frames;
    const int replacement = cloneFrame(rom, target, source, art, token, sprites, frames);
    const auto operands = duration ? record.operands : destination[targetRecord].operands;
    bank.setFrame(targetPose, targetRecord, replacement, u8(operands, operands.size() - 1));
    target.setPoses(bank);
    return target;
}
QPoint animationLayerTranslation(QPoint offset, int scaleX, int scaleY) {
    return {scaleX ? qRound(offset.x() * 1024. / scaleX) : 0,
            scaleY ? qRound(offset.y() * 1024. / scaleY) : 0};
}
AnimationDescriptor translateAnimationFrames(AnimationDescriptor target, const QMap<int, QSet<int>> &selected,
                                               QPoint offset, int layer, bool sceneSpace) {
    auto bank = target.poses();
    for (auto pose = selected.cbegin(); pose != selected.cend(); ++pose) {
        auto records = bank.effective(pose.key()).records;
        for (const int index : pose.value()) {
            if (index < 0 || index >= records.size() || (records[index].opcode != 1 && records[index].opcode != 21))
                throw Error("Choose frame instructions to translate");
            auto &record = records[index];
            const int token = u8(record.operands, 0) + (record.opcode == 21 ? 256 * u8(record.operands, 1) : 0);
            auto row = target.frame(token);
            if (layer >= u16(row, 0)) throw Error("The selected layer is absent from a selected frame");
            for (int i = 0; i < u16(row, 0); ++i) if (layer < 0 || layer == i) {
                const int at = 2 + i * 16;
                const auto delta = sceneSpace ? animationLayerTranslation(offset, u16(row, at + 12), u16(row, at + 14)) : offset;
                const int x = qint16(u16(row, at + 2)) + delta.x(), y = qint16(u16(row, at + 4)) + delta.y();
                if (x < -32768 || x > 32767 || y < -32768 || y > 32767)
                    throw Error("Translated layer exceeds signed sprite coordinates");
                put16(row, at + 2, quint16(x)); put16(row, at + 4, quint16(y));
            }
            const int replacement = target.appendFrame(row);
            record = {21, QByteArray(1, char(replacement)) + QByteArray(1, char(replacement >> 8)) + record.operands.right(1)};
        }
        bank.setProgram(pose.key(), PoseBank::encodeProgram(records));
    }
    target.setPoses(bank);
    return target;
}
} // namespace lordly
