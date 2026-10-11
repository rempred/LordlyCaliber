#include "engine/scene_resources.h"
#include <cstring>
namespace lordly {
SceneResources::SceneResources(SceneResourceContext context) {
    setContext(context);
}
void SceneResources::setContext(SceneResourceContext context) {
    pool_.margin = context.margin;
    pool_.context = context.context;
    pool_.parent = context.parent;
}
void SceneResources::checkSlot(int slot) {
    if (slot < 0 || slot >= LORDLY_UI_SLOTS)
        throw Error("Scene resource slot is outside 0..5");
}
QByteArray SceneResources::record(int slot) const {
    checkSlot(slot);
    return QByteArray(reinterpret_cast<const char *>(pool_.records[slot].bytes),
                      LORDLY_UI_RECORD_BYTES);
}
void SceneResources::supplyRecord(int slot, const QByteArray &record) {
    checkSlot(slot);
    if (record.size() != LORDLY_UI_RECORD_BYTES)
        throw Error("Scene resource requires its complete 168-byte record");
    std::memcpy(pool_.records[slot].bytes, record.constData(), LORDLY_UI_RECORD_BYTES);
}
bool SceneResources::active(int slot) const {
    return (u16(record(slot), 0) & 0x8000) != 0;
}
void SceneResources::rebuildPriority(SceneResourceQueue &queue) {
    lordly_ui_priority(&pool_, &queue);
}
void SceneResources::advanceOpening(const SceneResourceQueue &queue) {
    auto next = pool_;
    const auto result = lordly_ui_opening(&next, &queue);
    if (result)
        throw Error(result == -2 ? "Scene resource opening divides by zero speed"
                                 : "Scene resource queue exceeds the six-slot pool");
    pool_ = next;
}
bool SceneResources::advanceClosing(int slot) {
    checkSlot(slot);
    auto next = pool_.records[slot];
    const auto result = lordly_ui_closing(&next, pool_.margin);
    if (result < 0)
        throw Error("Scene resource closing divides by zero speed");
    pool_.records[slot] = next;
    return result != 0;
}
QVector<quint32> SceneResources::release(int slot, const Release &callback) {
    checkSlot(slot);
    auto next = *this;
    QVector<quint32> freed;
    unsigned visiting = 0;
    std::function<void(int)> releaseSlot = [&](int current) {
        if (visiting & (1U << current))
            throw Error("Scene resource parent links form a release cycle");
        visiting |= 1U << current;
        if (u8(next.record(current), 3) & 8) {
            for (int child = 0; child < 6; ++child) {
                if (child == current)
                    continue;
                const auto record = next.record(child);
                if ((u16(record, 0) & 0x8000) && qint16(u16(record, 0xa2)) == current)
                    releaseSlot(child);
            }
        }
        const auto address = u32(next.record(current), 0x1c);
        if (address) {
            if (!callback)
                throw Error(QString("Scene release callback %1 is unavailable")
                                .arg(address, 8, 16, QLatin1Char('0')));
            callback(next, current, address);
        }
        for (int i = 0; i < 10; ++i) {
            const auto handle = u32(next.record(current), 0x38 + i * 4);
            if (handle)
                freed.push_back(handle);
        }
        freed.push_back(u32(next.record(current), 0x24)); // Native payload-free is unconditional.
        next.supplyRecord(current, QByteArray(168, 0));
        visiting &= ~(1U << current);
    };
    releaseSlot(slot);
    *this = std::move(next);
    return freed;
}
int SceneResources::registerResource(quint16 flags, quint32 initializer, qint32 x, qint32 y,
                                     qint32 right, qint32 bottom, qint32 priority) {
    const int slot = lordly_ui_register(&pool_, flags, initializer, quint16(x), quint16(y),
                                        quint16(right), quint16(bottom), quint16(priority));
    if (slot < 0)
        throw Error("All six scene resource slots are occupied");
    return slot;
}
int SceneResources::registerWindow(quint32 initializer, qint32 x, qint32 y, qint32 kind,
                                   qint32 style) {
    const int slot = lordly_ui_window(&pool_, initializer, quint16(x), quint16(y), quint16(kind),
                                      quint16(style));
    if (slot < 0)
        throw Error("All six scene resource slots are occupied");
    return slot;
}
} // namespace lordly
