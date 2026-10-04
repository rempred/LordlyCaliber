#pragma once
#include "engine/actor_instance.h"
#include "engine/actor_motion.h"
#include <array>
#include <optional>
namespace lordly {
struct ActorBindingInputs {
    std::array<QByteArray, 20> rows;
    std::array<quint8, 5> unitMembers{};
};
class SceneActors {
  public:
    SceneActors(const Rom &rom, int mode, quint8 presentation, ActorInstance::Height height = {});
    const std::optional<ActorInstance> &actor(int slot) const;
    const LordlyActorMotion &motion(int slot) const;
    void supplyActor(int slot, const QByteArray &record);
    void supplyMotion(int slot, const LordlyActorMotion &motion);
    void supplyBindingInputs(ActorBindingInputs inputs);
    void vacate(int slot);
    bool service(quint32 opcode, const QVector<quint32> &operands, int &value);
    // Components of the normal scheduler. The caller must retain the game's
    // other services/order, including terrain and proximity, before advancing.
    void advancePlanar();
    void advancePoses();

  private:
    static void checkSlot(int slot);
    void bindMember(int destination, int member);
    Rom rom_;
    int mode_;
    quint8 presentation_;
    ActorInstance::Height height_;
    LordlyMotionConstants constants_;
    std::array<std::optional<ActorInstance>, 28> actors_;
    std::array<LordlyActorMotion, 28> motions_{};
    std::optional<ActorBindingInputs> binding_;
};
} // namespace lordly
