#pragma once
#include "engine/rom.h"
#include <QSet>
#include <QVector>
#include <array>
namespace lordly {
class ActorMaterials {
  public:
    const QByteArray &tint(int slot) const;
    const QByteArray &material(int slot) const;
    void supplyTint(int slot, QByteArray);
    void supplyMaterial(int slot, QByteArray);
    void startTint(const QVector<quint32> &, const QSet<int> &occupied);
    void startMaterial(const QVector<quint32> &);
    bool active(int slot) const;
    void releaseTint(int slot);
    void releaseMaterial(int slot);
    // Called exactly once by an eligible Actor draw. Updates only actor RGB and
    // tint progress; emits the original ordered material commands for rendering.
    QByteArray render(QByteArray &actor);

  private:
    static void check(int slot);
    std::array<QByteArray, 28> tints_, materials_;
};
} // namespace lordly
