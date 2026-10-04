#pragma once
#include "engine/rom.h"
#include "game/director/ui_resource.h"
namespace lordly {
struct SceneResourceContext {
    qint32 margin, context;
    quint16 parent;
};
// Shared by Director, dialogue and other callbacks. Native initializer values
// are identities only; the desktop dispatcher never calls them as pointers.
class SceneResources {
  public:
    explicit SceneResources(SceneResourceContext context);
    void setContext(SceneResourceContext context);
    QByteArray record(int slot) const;
    void supplyRecord(int slot, const QByteArray &record);
    bool active(int slot) const;
    int registerResource(quint16 flags, quint32 initializer, qint32 x, qint32 y, qint32 right,
                         qint32 bottom, qint32 priority);
    int registerWindow(quint32 initializer, qint32 x, qint32 y, qint32 kind, qint32 style);

  private:
    static void checkSlot(int slot);
    LordlyUiPool pool_{};
};
} // namespace lordly
