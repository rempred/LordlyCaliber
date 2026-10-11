#pragma once
#include "engine/scene_resources.h"
#include <QPointF>
#include <QVector>
#include <array>
#include <functional>
#include <optional>
namespace lordly {
// Window registration, Director tracking and placement. Text interpretation,
// opening/closing queues and shared name context have separate owners.
class SceneDialogue {
  public:
    using Project = std::function<std::optional<QPointF>(int actorSlot)>;
    using Portrait = std::function<qint32(int actorSlot)>;
    explicit SceneDialogue(quint8 sharedStatus) : sharedStatus_(sharedStatus) {}
    int create(const QVector<quint32> &args, SceneResources &resources, const Project &project = {},
               const Portrait &portrait = {});
    void updatePlacement(SceneResources &resources, const Project &project = {}) const;
    void close(qint32 window, SceneResources &resources);
    void resume(quint32 window, SceneResources &resources);
    int query(quint32 window, const SceneResources &resources) const;
    void setSharedStatus(quint8 value) {
        sharedStatus_ = value;
    }
    void setControl(int index, quint32 value);
    quint8 control(int index) const;
    const QByteArray &tracking(int actorSlot) const;
    void supplyTracking(int actorSlot, QByteArray record);

  private:
    static int find(quint8 window, const SceneResources &resources);
    static void checkActor(int actorSlot);
    void place(SceneResources &, const Project &) const;
    quint8 sharedStatus_;
    std::array<quint8, 8> controls_{};
    std::array<QByteArray, 28> tracking_;
};
} // namespace lordly
