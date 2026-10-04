#include "engine/scene_draw_queue.h"
#include <bit>
namespace lordly {
SceneDrawQueue SceneDrawQueue::build(const std::array<QByteArray, 28> &actors,
                                     const QVector<QByteArray> &objects, const SceneCamera &camera,
                                     LordlySceneDrawLayout layout) {
    if (objects.size() > 256)
        throw Error("A scene draw queue supports at most 256 scene objects");
    const uint8_t *actorPointers[28]{}, *objectPointers[256]{};
    auto bind = [](const QByteArray &record, int minimum) -> const uint8_t * {
        if (record.isEmpty())
            return nullptr;
        if (record.size() < minimum)
            throw Error("Truncated scene draw-queue input");
        return reinterpret_cast<const uint8_t *>(record.constData());
    };
    for (int i = 0; i < 28; ++i)
        actorPointers[i] = bind(actors[i], 336);
    for (int i = 0; i < objects.size(); ++i)
        objectPointers[i] = bind(objects[i], 21);
    float bank[14];
    for (int i = 0; i < 14; ++i)
        bank[i] = std::bit_cast<float>(u32(camera.bank(), i * 4));
    LordlySceneDrawEntry entries[284];
    int count = 0;
    if (!lordly_scene_draw_queue(entries, &count, actorPointers, objectPointers,
                                 int(objects.size()), bank, layout))
        throw Error("Scene draw-queue coordinates or camera are invalid");
    SceneDrawQueue result;
    result.entries_ = QVector<LordlySceneDrawEntry>(entries, entries + count);
    return result;
}
QByteArray SceneDrawQueue::record() const {
    QByteArray result(entries_.size() * 20, 0);
    for (int i = 0; i < entries_.size(); ++i) {
        const auto &r = entries_[i];
        put32(result, i * 20, quint32(r.x));
        put32(result, i * 20 + 4, quint32(r.y));
        put32(result, i * 20 + 8, quint32(r.z));
        put32(result, i * 20 + 12, quint32(r.depth));
        put16(result, i * 20 + 16, quint16(r.kind));
        put16(result, i * 20 + 18, quint16(r.index));
    }
    return result;
}
SceneDrawQueue SceneDrawQueue::fromRecord(const QByteArray &record) {
    if (record.size() % 20 || record.size() > 284 * 20)
        throw Error("Invalid scene draw queue record size");
    SceneDrawQueue result;
    for (int offset = 0; offset < record.size(); offset += 20)
        result.entries_.append({qint32(u32(record, offset)), qint32(u32(record, offset + 4)),
                                qint32(u32(record, offset + 8)), qint32(u32(record, offset + 12)),
                                qint16(u16(record, offset + 16)),
                                qint16(u16(record, offset + 18))});
    return result;
}
QVector<LordlySceneWorldDrawPass>
SceneDrawQueue::modeTwoPasses(const std::array<QByteArray, 28> &actors,
                              const QVector<QByteArray> &objects,
                              std::array<qint16, 2> shadowAlpha) const {
    if (objects.size() > 256)
        throw Error("World drawing supports at most256 objects");
    const uint8_t *actorPointers[28]{}, *objectPointers[256]{};
    for (int i = 0; i < 28; ++i) {
        if (actors[i].isEmpty())
            continue;
        if (actors[i].size() != 336)
            throw Error("Invalid Actor record in world draw queue");
        actorPointers[i] = reinterpret_cast<const uint8_t *>(actors[i].constData());
    }
    for (int i = 0; i < objects.size(); ++i) {
        if (objects[i].isEmpty())
            continue;
        if (objects[i].size() < 21)
            throw Error("Invalid scene object in world draw queue");
        objectPointers[i] = reinterpret_cast<const uint8_t *>(objects[i].constData());
    }
    LordlySceneWorldDrawPass passes[852];
    int count = 0;
    if (!lordly_scene_mode_two_passes(passes, &count, entries_.constData(), int(entries_.size()),
                                      actorPointers, objectPointers, shadowAlpha.data()))
        throw Error("World draw queue refers to an absent or invalid Actor/object");
    return QVector<LordlySceneWorldDrawPass>(passes, passes + count);
}
QVector<LordlySceneDrawPass>
SceneDrawQueue::modeZeroPasses(const std::array<QByteArray, 28> &actors, quint8 objectChannel,
                               quint8 channel) const {
    const uint8_t *pointers[28]{};
    for (int i = 0; i < 28; ++i) {
        if (actors[i].isEmpty())
            continue;
        if (actors[i].size() != 336)
            throw Error("Invalid Actor record in draw queue");
        pointers[i] = reinterpret_cast<const uint8_t *>(actors[i].constData());
    }
    LordlySceneDrawPass passes[568];
    int count = 0;
    if (!lordly_scene_mode_zero_passes(passes, &count, entries_.constData(), int(entries_.size()),
                                       pointers, objectChannel, channel))
        throw Error("Scene draw queue refers to an absent or invalid Actor/object");
    return QVector<LordlySceneDrawPass>(passes, passes + count);
}
} // namespace lordly
