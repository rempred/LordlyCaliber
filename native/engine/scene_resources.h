#pragma once
#include "engine/rom.h"
#include "game/director/ui_resource.h"
#include <QVector>
#include <functional>
namespace lordly {
struct SceneResourceContext {
    qint32 margin, context;
    quint16 parent;
};
using SceneResourceQueue = LordlyUiQueue;
// Shared by Director, dialogue and other callbacks. Native initializer values
// are identities only; the desktop dispatcher never calls them as pointers.
class SceneResources {
  public:
    explicit SceneResources(SceneResourceContext context);
    void setContext(SceneResourceContext context);
    QByteArray record(int slot) const;
    void supplyRecord(int slot, const QByteArray &record);
    bool active(int slot) const;
    void rebuildPriority(SceneResourceQueue &queue);
    void advanceOpening(const SceneResourceQueue &queue);
    // True means the owner must run this slot's release before the next queue entry.
    bool advanceClosing(int slot);
    using Release = std::function<void(SceneResources &, int slot, quint32 callback)>;
    // Callback operates on the staged pool. Callers stage any other state it
    // touches as well. Returned native handles are identities, never PC pointers.
    QVector<quint32> release(int slot, const Release &callback = {});
    int registerResource(quint16 flags, quint32 initializer, qint32 x, qint32 y, qint32 right,
                         qint32 bottom, qint32 priority);
    int registerWindow(quint32 initializer, qint32 x, qint32 y, qint32 kind, qint32 style);

  private:
    static void checkSlot(int slot);
    LordlyUiPool pool_{};
};
} // namespace lordly
