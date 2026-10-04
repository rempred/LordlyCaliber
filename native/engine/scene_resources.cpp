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
