#include "engine/scene_actors.h"
#include "engine/class_data.h"
#include "game/director/actor_body.h"
#include <bit>
#include <cmath>
#include <cstring>
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
void SceneActors::setMode(int mode) {
    if (mode < 0 || mode > 2)
        throw Error("Actor scene mode is outside 0..2");
    mode_ = mode;
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
const ActorHeightJob &SceneActors::heightJob(int slot) const {
    checkSlot(slot);
    return heights_[slot];
}
void SceneActors::supplyHeightJob(int slot, ActorHeightJob job) {
    checkSlot(slot);
    heights_[slot] = std::move(job);
}
void SceneActors::supplyBindingInputs(ActorBindingInputs inputs) {
    for (const auto &row : inputs.rows)
        if (row.size() != 248)
            throw Error("Actor binding requires twenty complete 248-byte caller rows");
    for (int i = 0; i < 20; ++i)
        bodyRows_[i] = inputs.rows[i];
    binding_ = std::move(inputs);
}
void SceneActors::supplyBodyRows(const QVector<QByteArray> &rows) {
    if (rows.size() > 28)
        throw Error("Body context exceeds 28 actor identities");
    for (const auto &row : rows)
        if (row.size() != 248)
            throw Error("Body context requires complete 248-byte rows");
    for (int i = 0; i < 28; ++i)
        bodyRows_[i] = i < rows.size() ? rows[i] : QByteArray{};
}
const QByteArray &SceneActors::bodyRow(int identity) const {
    if (identity < 0 || identity >= 28 || bodyRows_[identity].size() != 248)
        throw Error("Body command requires its caller appearance row");
    return bodyRows_[identity];
}
int SceneActors::classification(const QByteArray &record) const {
    const int identity = u8(record, 0x147);
    if (identity == 255)
        return 0;
    const auto &row = bodyRow(identity);
    return lordly_body_classification(reinterpret_cast<const uint8_t *>(row.constData()),
                                      u8(record, 0x149));
}
void SceneActors::configureLinkedPose(ActorInstance &actor) const {
    if (!actor.pose().actor().decoder_mode)
        return;
    const auto record = actor.record();
    const auto &row = bodyRow(u8(record, 0x147));
    const int member = u8(record, 0x149);
    if (member >= 3)
        throw Error("Body pose linked-object selector exceeds its three entries");
    const auto object = u32(row, member * 4);
    if (object)
        actor.pose().setLinkedAudioContext(
            {object, qint32(u32(row, 0x48)), qint32(u32(row, 0x4c))});
    else
        actor.pose().setLinkedObjectKnownAbsent(true);
}
void SceneActors::selectBody(const LordlyActorBodySelection &selection) {
    auto next = *this;
    next.registrations_.clear();
    next.selectBodyInternal(selection);
    *this = std::move(next);
}
void SceneActors::selectBodyInternal(const LordlyActorBodySelection &s) {
    if (s.slot == -1)
        return;
    checkSlot(s.slot);
    if (!actors_[s.slot])
        return;
    if (actors_[s.slot]->pose().suspended())
        throw Error("Finish the actor's pose invocation before changing its body");
    auto record = actors_[s.slot]->record();
    LordlyActorRecord raw;
    std::memcpy(raw.bytes, record.constData(), sizeof(raw.bytes));
    int advance = 0;
    if (lordly_actor_body_select(&raw, &s, classification(record), &advance) != LORDLY_ACTOR_OK)
        throw Error("Invalid body selection");
    if (!advance)
        return;
    const int flagB = s.flag_b == -1 ? qint16(u16(record, 0x13a)) : s.flag_b;
    if (flagB < 0 || flagB > 1)
        throw Error("Body registration flag exceeds 0..1");
    record = QByteArray(reinterpret_cast<const char *>(raw.bytes), sizeof(raw.bytes));
    int identity = u8(record, 0x147);
    if (!s.keep_identity && identity == 255) {
        std::array<bool, 28> used{};
        for (const auto &a : actors_)
            if (a) {
                const int id = u8(a->record(), 0x147);
                if (id < 28)
                    used[id] = true;
            }
        identity = 0;
        while (identity < 28 && used[identity])
            ++identity;
        if (identity == 28)
            throw Error("No free body appearance identity");
        bodyRow(identity); // Validate before replacing appearance fields.
        put8(record, 0x147, quint8(identity));
        put8(record, 0x148, 1);
        const auto classes = ClassData::load(rom_);
        QByteArray records;
        for (const auto &entry : classes.entries())
            records += entry.record;
        uint16_t appearance[4];
        if (lordly_body_appearance(reinterpret_cast<const uint8_t *>(records.constData()),
                                   int(classes.entries().size()), qint32(u32(record, 0xe8)),
                                   qint32(u32(record, 0xec)), appearance))
            throw Error("Body appearance class exceeds the current class table");
        for (int i = 0; i < 4; ++i)
            put16(bodyRows_[identity], 0x36 + i * 2, appearance[i]);
    }
    const auto &row = bodyRow(identity);
    uint16_t appearance[4], equipment;
    for (int i = 0; i < 4; ++i)
        appearance[i] = u16(row, 0x36 + i * 2);
    const auto items = rom_.bytes().mid(0x62300, 278 * 32);
    if (items.size() != 278 * 32 ||
        lordly_body_equipment(reinterpret_cast<const uint8_t *>(items.constData()), 278, appearance,
                              &equipment))
        throw Error("Body appearance item exceeds the current item table");
    ActorInstance replacement(rom_, record);
    configureLinkedPose(replacement);
    const auto &p = replacement.pose().actor();
    registrations_ << BodyRegistration{s.slot,   p.bank,
                                       p.owner,  p.variant,
                                       p.flag_b, lordly_body_equipment_variant(p.bank, equipment)};
    if (replacement.pose().advance() != LORDLY_POSE_OK)
        throw Error(replacement.pose().error());
    actors_[s.slot] = std::move(replacement);
    if (classification(actors_[s.slot]->record()) == 1) {
        for (int i = 0; i < 28; ++i) {
            if (!actors_[i])
                continue;
            const auto candidate = actors_[i]->record();
            if (u8(candidate, 0x147) == identity && classification(candidate) == 2) {
                auto linked = s;
                linked.slot = i;
                selectBodyInternal(linked);
            }
        }
    }
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
        const auto &row = bodyRow(i);
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
    if (opcode == 0x1e) {
        QSet<int> occupied;
        for (int slot = 0; slot < 28; ++slot)
            if (actors_[slot])
                occupied.insert(slot);
        materials_.startTint(args, occupied);
        return true;
    }
    if (opcode == 0x1f) {
        if (args.size() != 3)
            throw Error("Actor tint query is truncated");
        value = materials_.active(qint32(args[2]));
        return true;
    }
    if (opcode == 0x28) {
        materials_.startMaterial(args);
        return true;
    }
    if (opcode == 0x29) {
        if (args.size() != 1)
            throw Error("Actor material release is truncated");
        materials_.releaseMaterial(qint32(args[0]));
        return true;
    }
    if (opcode == 0xa7) {
        if (args.size() != 1)
            throw Error("Actor tint cleanup is truncated");
        if (args[0] == 0xffffffffU)
            throw Error("Native all-actor tint cleanup crosses into adjacent scene tables; use "
                        "individual slots");
        const int first = qint16(args[0]), end = qint16(args[0] + 1U);
        if (first >= end)
            return true;
        checkSlot(first);
        materials_.releaseTint(first);
        if (actors_[first])
            actors_[first]->setTint(255, 255, 255);
        return true;
    }
    if (opcode == 0x3f) {
        if (args.size() != 1)
            throw Error("Actor entrance command is truncated");
        startEntrance();
        return true;
    }
    if (opcode == 0xa4) {
        if (args.size() != 1)
            throw Error("Actor return command is truncated");
        startReturn(qint32(args[0]));
        return true;
    }
    if (opcode == 0x40 || opcode == 0xa5) {
        if (args.size() != 2)
            throw Error("Actor presentation query is truncated");
        value = !presentationRecord_.isEmpty();
        return true;
    }
    if (opcode == 0x15) {
        if (args.size() != 5)
            throw Error("Actor turn command is truncated");
        const int slot = qint32(args[0]);
        checkSlot(slot);
        turns_[slot].start(rom_, qint32(args[1]), qint32(args[2]), qint32(args[3]),
                           qint32(args[4]));
        return true;
    }
    if (opcode == 0x16) {
        if (args.size() != 3)
            throw Error("Actor turn query is truncated");
        const int slot = qint32(args[2]);
        checkSlot(slot);
        value = turns_[slot].present() ? 1 : 0;
        return true;
    }
    if (opcode == 0x7b) {
        if (args.size() != 4)
            throw Error("Actor height command is truncated");
        const int slot = qint32(args[0]);
        checkSlot(slot);
        heights_[slot].start(rom_, qint32(args[1]), qint32(args[2]), qint32(args[3]),
                             actors_[slot]
                                 ? std::optional<float>(actors_[slot]->pose().actor().position.y)
                                 : std::nullopt);
        return true;
    }
    if (opcode == 0x1d || opcode == 0x48) {
        if (args.size() != 2)
            throw Error("Actor scale/opacity command is truncated");
        const int requested = qint32(args[0]);
        const int first = requested == -1 ? 0 : qint16(args[0]);
        const int end = requested == -1 ? 28 : qint16(args[0] + 1U);
        if (first >= end)
            return true;
        checkSlot(first);
        checkSlot(end - 1);
        float scale = 0;
        if (opcode == 0x1d) {
            const auto &bytes = rom_.bytes();
            const double divisor =
                std::bit_cast<double>(quint64(u32(bytes, 0x2a8b80)) << 32 | u32(bytes, 0x2a8b84));
            if (!std::isfinite(divisor) || divisor == 0)
                throw Error("Actor scale divisor is invalid");
            scale = float(double(float(qint32(args[1]))) / divisor);
            if (!std::isfinite(scale))
                throw Error("Actor scale exceeds the finite range");
        }
        for (int i = first; i < end; ++i)
            if (actors_[i]) {
                if (opcode == 0x1d)
                    actors_[i]->setUniformScale(scale);
                else
                    actors_[i]->setOpacity(quint8(args[1]));
            }
        return true;
    }
    if (opcode == 0x2a) {
        if (args.size() != 6)
            throw Error("Body selection command is truncated");
        if (mode_ == 2)
            selectBody({qint32(args[0]), qint32(args[1]), qint32(args[5]), qint32(args[2]),
                        qint32(args[3]), qint32(args[4]), 0, 0, presentation_});
        return true;
    }
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
QByteArray SceneActors::renderMaterials(int slot) {
    checkSlot(slot);
    if (!actors_[slot])
        throw Error("Material rendering requires an occupied actor slot");
    auto record = actors_[slot]->record();
    const auto commands = materials_.render(record);
    actors_[slot]->setTint(u8(record, 0x142), u8(record, 0x143), u8(record, 0x144));
    return commands;
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
void SceneActors::advanceHeights() {
    auto next = actors_;
    auto heights = heights_;
    for (int i = 0; i < 28; ++i) {
        const auto step = heights[i].advance();
        if (!step)
            continue;
        if (!next[i])
            throw Error("Actor height job requires an occupied slot when applying its step");
        next[i]->addHeight(*step);
    }
    actors_ = std::move(next);
    heights_ = std::move(heights);
}
const ActorTurnJob &SceneActors::turnJob(int slot) const {
    checkSlot(slot);
    return turns_[slot];
}
void SceneActors::supplyTurnJob(int slot, ActorTurnJob job) {
    checkSlot(slot);
    turns_[slot] = std::move(job);
}
void SceneActors::advanceTurns() {
    auto next = actors_;
    auto turns = turns_;
    for (int slot = 0; slot < 28; ++slot) {
        const auto facing = turns[slot].advance(rom_);
        if (!facing)
            continue;
        if (!next[slot])
            throw Error("Actor turn requires an occupied slot when selecting its pose");
        next[slot]->select({slot, -1, -1, *facing, -1, {-1, -1, -1}, 0, 0});
    }
    actors_ = std::move(next);
    turns_ = std::move(turns);
}
bool SceneActors::advancePoses(int instructionBudget) {
    if (instructionBudget < 1)
        throw Error("Actor pose budget must be positive");
    for (; nextPoseSlot_ < 28; ++nextPoseSlot_) {
        auto &actor = actors_[nextPoseSlot_];
        if (!actor)
            continue;
        configureLinkedPose(*actor);
        const int status = actor->pose().advance(instructionBudget);
        if (status == LORDLY_POSE_BUDGET)
            return false;
        if (status != LORDLY_POSE_OK)
            throw Error(actor->pose().error());
    }
    nextPoseSlot_ = 0;
    return true;
}
} // namespace lordly
