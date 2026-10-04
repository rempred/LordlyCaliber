#pragma once
#include "engine/sprite_pose.h"
#include <array>
#include <functional>
#include <optional>
namespace lordly {
class SceneSprites {
  public:
    // The path owner resolves sampled/resource headings and owns the native
    // temporary resource-path slot 19 lifecycle. Declining must not mutate it.
    using Rotation = std::function<qint16(qint32 group, qint32 entry)>;
    explicit SceneSprites(const Rom &rom) : rom_(rom) {}
    bool service(quint32 opcode, const QVector<quint32> &args, int &value);
    bool advancePoses(int instructionBudget = 256);
    const SpritePosePlayer *sprite(int slot) const;
    void supplyRecord(int slot, const QByteArray &record);
    void setRotationSource(Rotation source) {
        rotation_ = std::move(source);
    }

  private:
    static void checkSlot(int slot);
    SpritePosePlayer create(const QVector<quint32> &args) const;
    Rom rom_;
    std::array<std::optional<SpritePosePlayer>, 30> sprites_;
    int nextPoseSlot_ = 0;
    Rotation rotation_;
};
} // namespace lordly
