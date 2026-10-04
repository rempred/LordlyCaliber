#pragma once
#include "engine/actor_height.h"
#include "engine/actor_instance.h"
#include "engine/actor_motion.h"
#include <array>
#include <optional>
namespace lordly {
struct ActorBindingInputs {
    std::array<QByteArray, 20> rows;
    std::array<quint8, 5> unitMembers{};
};
struct BodyRegistration {
    int slot, source, owner, flagA, flagB, equipment;
};
class SceneActors {
  public:
    SceneActors(const Rom &rom, int mode, quint8 presentation, ActorInstance::Height height = {});
    void setMode(int mode);
    const std::optional<ActorInstance> &actor(int slot) const;
    const LordlyActorMotion &motion(int slot) const;
    void supplyActor(int slot, const QByteArray &record);
    void supplyMotion(int slot, const LordlyActorMotion &motion);
    const ActorHeightJob &heightJob(int slot) const;
    void supplyHeightJob(int slot, ActorHeightJob job);
    void supplyBindingInputs(ActorBindingInputs inputs);
    // Explicit launch state: absent rows remain unknown, never silently zero-filled.
    void supplyBodyRows(const QVector<QByteArray> &rows);
    const QByteArray &bodyRow(int identity) const;
    const QVector<BodyRegistration> &bodyRegistrations() const {
        return registrations_;
    }
    void selectBody(const LordlyActorBodySelection &selection);
    void vacate(int slot);
    bool service(quint32 opcode, const QVector<quint32> &operands, int &value);
    // Components of the normal scheduler. The caller must retain the game's
    // other services/order, including terrain and proximity, before advancing.
    void advancePlanar();
    void advanceHeights();
    void advancePoses();

  private:
    static void checkSlot(int slot);
    void bindMember(int destination, int member);
    int classification(const QByteArray &record) const;
    void selectBodyInternal(const LordlyActorBodySelection &selection);
    void configureLinkedPose(ActorInstance &actor) const;
    Rom rom_;
    int mode_;
    quint8 presentation_;
    ActorInstance::Height height_;
    LordlyMotionConstants constants_;
    std::array<std::optional<ActorInstance>, 28> actors_;
    std::array<LordlyActorMotion, 28> motions_{};
    std::array<ActorHeightJob, 28> heights_;
    std::optional<ActorBindingInputs> binding_;
    std::array<QByteArray, 28> bodyRows_;
    QVector<BodyRegistration> registrations_;
};
} // namespace lordly
