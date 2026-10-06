#pragma once
#include "engine/actor_divider.h"
#include "engine/actor_height.h"
#include "engine/actor_instance.h"
#include "engine/actor_materials.h"
#include "engine/actor_motion.h"
#include "engine/actor_turn.h"
#include "engine/scene_draw_queue.h"
#include <QHash>
#include <array>
#include <optional>
namespace lordly {
class SceneTransforms;
struct ActorBindingInputs {
    std::array<QByteArray, 20> rows;
    std::array<quint8, 5> unitMembers{};
    bool unitMembersKnown = true;
};
struct BodyRegistration {
    int slot, source, owner, flagA, flagB, equipment;
};
struct PresentationMemberState {
    quint16 value = 0;
    quint8 flags = 0;
};
struct PresentationRoster {
    quint8 overrideFlag = 0;
    std::array<quint8, 2> sideGroups{};
    // Native member tables selected by group <30 / >=30, keyed by member ID.
    std::array<QHash<int, PresentationMemberState>, 2> members;
};
struct PresentationLaunch {
    qint32 preset = 0;
    qint8 variant = 0;
    std::optional<PresentationRoster> roster;
};
struct ActorRosterLaunch {
    qint8 context = 0;
    qint16 scenario = 0;
    std::optional<PresentationRoster> roster;
};
class SceneActors {
  public:
    SceneActors(const Rom &rom, int mode, quint8 presentation, ActorInstance::Height height = {},
                BodyAnimationData::Pointer data = {});
    void setMode(int mode);
    const std::optional<ActorInstance> &actor(int slot) const;
    qint32 dialoguePortrait(int slot) const;
    const LordlyActorMotion &motion(int slot) const;
    void supplyActor(int slot, const QByteArray &record);
    void supplyMotion(int slot, const LordlyActorMotion &motion);
    void allowCapturedBodyPreview(bool enabled) { capturedBodyPreview_ = enabled; }
    const ActorHeightJob &heightJob(int slot) const;
    void supplyHeightJob(int slot, ActorHeightJob job);
    void supplyBindingInputs(ActorBindingInputs inputs);
    // Explicit launch state: absent rows remain unknown, never silently zero-filled.
    void supplyBodyRows(const QVector<QByteArray> &rows);
    const QByteArray &bodyRow(int identity) const;
    quint16 bodyEquipment(int slot) const;
    // Native pointers identify launch objects; they are never dereferenced on PC.
    void supplyBodyObject(quint32 address, QByteArray record);
    const QByteArray &presentationRecord() const {
        return presentationRecord_;
    }
    void supplyPresentationRecord(QByteArray record);
    void startReturn(int group);
    void supplyPresentationLaunch(PresentationLaunch launch) {
        presentationLaunch_ = std::move(launch);
    }
    void startEntrance();
    void supplyRosterLaunch(ActorRosterLaunch launch) {
        rosterLaunch_ = std::move(launch);
    }
    void materializeRoster(qint32 first, qint32 second, int variant);
    const QVector<int> &immediateRosterDraws() const {
        return rosterDraws_;
    }
    std::optional<quint8> presentationOpacity() const {
        return presentationOpacity_;
    }
    void advancePresentation();
    float bodySpeed(int source, int owner, int direction) const;
    const QVector<BodyRegistration> &bodyRegistrations() const {
        return registrations_;
    }
    void selectBody(const LordlyActorBodySelection &selection);
    void vacate(int slot);
    bool service(quint32 opcode, const QVector<quint32> &operands, int &value);
    const ActorMaterials &materials() const {
        return materials_;
    }
    QByteArray renderMaterials(int slot);
    const ActorDivider &divider() const {
        return divider_;
    }
    void supplyDivider(ActorDivider divider) {
        divider_ = std::move(divider);
    }
    void advanceDivider();
    void prepareModeZeroMatrices(const SceneCamera &, const SceneTransforms &);
    void prepareModeTwoMatrices(const SceneCamera &, const LordlyActorLighting &);
    QPointF projectPosition(int slot, SceneVector position, const SceneCameras &,
                            const SceneTransforms &,
                            const std::optional<LordlyActorLighting> &) const;
    SceneVector positionAt(int slot, QPointF screen, const SceneCameras &, const SceneTransforms &,
                           const std::optional<LordlyActorLighting> &) const;
    std::array<QByteArray, 28> actorRecords() const;
    quint8 defaultChannel() const {
        return defaultChannel_;
    }
    // Components of the normal scheduler. The caller must retain the game's
    // other services/order, including terrain and proximity, before advancing.
    void advancePlanar();
    void advanceMovement();
    void supplySceneFlags(quint32 flags) {
        sceneFlags_ = flags;
    }
    void supplyDepthAttachment(int slot, QByteArray record);
    const QByteArray &depthAttachment(int slot) const;
    void advanceHeights();
    void advanceTurns();
    const ActorTurnJob &turnJob(int slot) const;
    void supplyTurnJob(int slot, ActorTurnJob job);
    // A false result retains the current slot/invocation for a budget resume.
    bool advancePoses(int instructionBudget = 256);

  private:
    static void checkSlot(int slot);
    void bindMember(int destination, int member);
    void bindRow(int destination, int ordinal);
    int classification(const QByteArray &record) const;
    void selectBodyInternal(const LordlyActorBodySelection &selection);
    void configureLinkedPose(ActorInstance &actor) const;
    std::array<float, 2> presentationTarget(const QByteArray &actor) const;
    void returnToBodyPosition(int slot);
    bool presentationMemberUnavailable(const QByteArray &row) const;
    bool rosterMemberUnavailable(const QByteArray &, const PresentationRoster &) const;
    std::array<int, 4> spawnBodyRow(int identity, bool previousSpecial, int direction = -1);
    Rom rom_;
    int bodyConstruction(int source) const;
    BodyAnimationData::Pointer bodyData_;
    ActorMaterials materials_;
    ActorDivider divider_;
    quint8 defaultChannel_ = 0;
    int mode_;
    quint8 presentation_;
    ActorInstance::Height height_;
    LordlyMotionConstants constants_;
    std::array<std::optional<ActorInstance>, 28> actors_;
    std::array<LordlyActorMotion, 28> motions_{};
    std::array<ActorHeightJob, 28> heights_;
    std::array<ActorTurnJob, 28> turns_;
    std::array<QByteArray, 28> depths_;
    std::optional<quint32> sceneFlags_;
    int nextPoseSlot_ = 0;
    std::optional<ActorBindingInputs> binding_;
    bool capturedBodyPreview_ = false;
    std::array<QByteArray, 28> bodyRows_;
    QVector<BodyRegistration> registrations_;
    QHash<quint32, QByteArray> bodyObjects_;
    QByteArray presentationRecord_;
    std::optional<PresentationLaunch> presentationLaunch_;
    std::optional<ActorRosterLaunch> rosterLaunch_;
    QVector<int> rosterDraws_;
    std::optional<quint8> presentationOpacity_;
};
} // namespace lordly
