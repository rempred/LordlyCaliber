#include "engine/scene_actors.h"
namespace lordly {
namespace {
float coordinate(quint32 word) {
    return float(qint32(word)) / 1000.f;
}
} // namespace
SceneActors::SceneActors(const Rom &rom, int mode, quint8 presentation,
                         ActorInstance::Height height)
    : rom_(rom), mode_(mode), presentation_(presentation), height_(std::move(height)),
      constants_(actorMotionConstants(rom)) {
    if (mode < 0 || mode > 2)
        throw Error("Actor scene mode is unknown");
}
void SceneActors::checkSlot(int slot) {
    if (slot < 0 || slot >= 28)
        throw Error("Actor slot is outside 0..27");
}
const std::optional<ActorInstance> &SceneActors::actor(int slot) const {
    checkSlot(slot);
    return actors_[slot];
}
const LordlyActorMotion &SceneActors::motion(int slot) const {
    checkSlot(slot);
    return motions_[slot];
}
void SceneActors::supplyActor(int slot, const QByteArray &record) {
    checkSlot(slot);
    actors_[slot] = ActorInstance(rom_, record);
}
void SceneActors::supplyMotion(int slot, const LordlyActorMotion &motion) {
    checkSlot(slot);
    motions_[slot] = motion;
}
void SceneActors::supplyBindingInputs(ActorBindingInputs inputs) {
    for (const auto &row : inputs.rows)
        if (row.size() != 248)
            throw Error("Actor binding requires twenty complete 248-byte caller rows");
    binding_ = std::move(inputs);
}
void SceneActors::vacate(int slot) {
    checkSlot(slot);
    actors_[slot].reset();
}
void SceneActors::bindMember(int destination, int member) {
    // func_002AD434 narrows the destination before indexing the scene.
    destination &= 255;
    checkSlot(destination);
    if (member < 0 || member >= 5 || !binding_)
        throw Error("Unit-member binding requires the current unit and complete caller rows");
    int ordinal = -1;
    for (int i = 0; i < 20; ++i) {
        const auto &row = binding_->rows[i];
        if ((u32(row, 0x40) & 0x100) && u8(row, 0xf6) == binding_->unitMembers[member] &&
            u32(row, 0x48)) {
            ordinal = i;
            break;
        }
    }
    if (ordinal < 0)
        return;
    for (int from = 0; from < 28; ++from) {
        if (!actors_[from] || u8(actors_[from]->record(), 0x147) != ordinal)
            continue;
        std::swap(actors_[from], actors_[destination]);
        actors_[destination]->setSlot(destination);
        if (actors_[from])
            actors_[from]->setSlot(from);
        return;
    }
}
bool SceneActors::service(quint32 opcode, const QVector<quint32> &args, int &value) {
    if (opcode == 0xa6 && args.size() == 2) {
        bindMember(qint32(args[0]), qint32(args[1]));
        return true;
    }
    if (opcode == 3 || opcode == 0x14) {
        const bool create = opcode == 0x14;
        if (args.size() != (create ? 9 : 8))
            throw Error("Actor State/creation command is truncated");
        const int slot = qint32(args[0]);
        if (!create && slot == -1)
            return true;
        checkSlot(slot);
        LordlyActorSelection selection{
            slot,
            qint32(args[1]),
            qint32(args[2]),
            qint32(args[3]),
            qint32(args[create ? 8 : 7]),
            {coordinate(args[4]), coordinate(args[5]), coordinate(args[6])},
            quint8(create ? args[7] : 0),
            presentation_};
        if (create)
            actors_[slot] = ActorInstance::create(rom_, selection, mode_, height_);
        else {
            if (!actors_[slot])
                throw Error("Actor State requires an occupied slot");
            actors_[slot]->select(selection);
        }
        return true;
    }
    if (opcode == 7) {
        if (args.size() != 7)
            throw Error("Actor movement command is truncated");
        const int slot = qint32(args[0]);
        checkSlot(slot);
        if (!actors_[slot])
            return true;
        auto terrain = [](void *opaque, int16_t x, int16_t z, float *height) -> int {
            auto &self = *static_cast<SceneActors *>(opaque);
            if (!self.height_)
                return 0;
            *height = self.height_(2, float(x), float(z));
            return 1;
        };
        const int status =
            lordly_actor_move(&actors_[slot]->pose().actor().position, &motions_[slot], &constants_,
                              coordinate(args[1]), coordinate(args[2]), coordinate(args[3]),
                              coordinate(args[4]), qint32(args[5]), qint32(args[6]), mode_,
                              u8(actors_[slot]->record(), 0x145), terrain, this);
        if (status != LORDLY_MOTION_OK)
            throw Error(status == LORDLY_MOTION_TERRAIN ? "Movement requires terrain height"
                                                        : "Movement produced invalid arithmetic");
        return true;
    }
    if (opcode == 0xf || opcode == 0x11) {
        if (args.size() != 3)
            throw Error("Actor query is truncated");
        const int slot = qint32(args[2]);
        checkSlot(slot);
        if (opcode == 0x11)
            value = motions_[slot].present ? qint16(motions_[slot].remaining) : 0;
        else {
            if (!actors_[slot])
                throw Error("Pose query requires an occupied slot");
            value = actors_[slot]->pose().query();
        }
        return true;
    }
    return false;
}
void SceneActors::advancePlanar() {
    auto next = actors_;
    auto motions = motions_;
    for (int i = 0; i < 28; ++i) {
        if (!next[i])
            continue;
        if (lordly_actor_advance_planar(&next[i]->pose().actor().position, &motions[i]) !=
            LORDLY_MOTION_OK)
            throw Error("Actor integration produced a nonfinite coordinate");
    }
    actors_ = std::move(next);
    motions_ = motions;
}
void SceneActors::advancePoses() {
    auto next = actors_;
    for (auto &actor : next)
        if (actor && actor->pose().advance() != LORDLY_POSE_OK)
            throw Error(actor->pose().error());
    actors_ = std::move(next);
}
} // namespace lordly
