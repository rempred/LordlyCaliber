#pragma once
#include "engine/sprite_pose.h"
#include <array>
#include <functional>
#include <optional>
namespace lordly {
struct SceneSpriteDraw {
    int slot = 0;
    quint8 pass = 0;
    float x = 0, y = 0, rotation = 0, scale = 1;
};
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
    void supplySprite(int slot, const SpritePosePlayer &);
    void setPass(int slot, quint8 pass);
    quint64 identity(int slot) const {
        checkSlot(slot);
        return identities_[slot];
    }
    void setRotationSource(Rotation source) {
        rotation_ = std::move(source);
    }
    // Native pool order and transform arguments, independent of PC rasterization.
    QVector<SceneSpriteDraw> draws(qint32 renderPass, quint8 special) const;
    // ROM 29F7A0..29F924 primitive RGBA, before texture blending.
    std::array<quint8, 4> primitiveColor(int slot, int layer, quint8 alpha,
                                         const std::array<qint16, 3> &globalRgb,
                                         const QByteArray &colorTransition = {}) const;

  private:
    static void checkSlot(int slot);
    SpritePosePlayer create(const QVector<quint32> &args) const;
    Rom rom_;
    std::array<std::optional<SpritePosePlayer>, 30> sprites_;
    std::array<quint64, 30> identities_{};
    quint64 nextIdentity_ = 0;
    int nextPoseSlot_ = 0;
    Rotation rotation_;
};
} // namespace lordly
