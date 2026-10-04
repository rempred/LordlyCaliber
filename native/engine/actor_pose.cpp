#include "engine/actor_pose.h"
#include <bit>
#include <cmath>
#include <cstring>
namespace lordly {
void ActorPosePlayer::setLinkedAudioContext(LinkedPoseAudioContext context) {
    if (!context.objectAddress)
        throw Error("Linked sound context requires its object identity");
    linkedAudioContext_ = context;
    linkedObjectAbsent_ = false;
}
ActorPosePlayer::ActorPosePlayer(const Rom &rom, ActorArt art, int state, int variant)
    : rom_(rom), art_(std::move(art)) {
    if (state < 0 || state >= art_.poses().size() || variant < 0 || variant > 255)
        throw Error("Invalid actor pose or variant");
    actor_.bank = art_.bank();
    actor_.state = qint16(state);
    actor_.variant = quint8(variant);
    actor_.cursor = -1;
    if (art_.isBody()) {
        actor_.bank = art_.sourceClass();
        actor_.owner = art_.ownerClass();
        actor_.variant = quint8(art_.flagA());
        actor_.flag_b = qint16(art_.flagB());
        actor_.decoder_mode = 1;
    }
    std::memset(actor_.material, 255, 16);
}
int ActorPosePlayer::lookup(void *context, const LordlyActorPose *actor, LordlyPoseRecord *record) {
    auto &self = *static_cast<ActorPosePlayer *>(context);
    try {
        if (self.art_.isBody()) {
            if (!actor->decoder_mode || actor->bank != self.art_.sourceClass() ||
                actor->owner != self.art_.ownerClass() || actor->variant != self.art_.flagA() ||
                actor->flag_b != self.art_.flagB())
                throw Error("This actor requires a different class body binding");
        } else if (actor->decoder_mode || actor->bank != self.art_.bank())
            throw Error("This actor requires a different pose resource binding");
        const auto decoded = self.art_.poseRecord(actor->state, actor->cursor);
        record->opcode = quint8(decoded.opcode);
        for (int i = 0; i < decoded.operands.size(); ++i)
            record->operands[i] = u8(decoded.operands, i);
        return 1;
    } catch (const Error &e) {
        self.error_ = QString::fromUtf8(e.what());
        return 0;
    }
}
int ActorPosePlayer::shared(void *context, const LordlyActorPose *actor,
                            const LordlyPoseRecord *record) {
    auto &self = *static_cast<ActorPosePlayer *>(context);
    try {
        if (actor->decoder_mode && (record->opcode == 18 || record->opcode == 20)) {
            if (self.linkedObjectAbsent_)
                return 1;
            if (self.linkedAudioContext_) {
                if (self.linkedAudio_.size() >= 4096)
                    throw Error("Linked pose sound request budget exceeded");
                self.linkedAudio_ << LinkedPoseAudioRequest{
                    *self.linkedAudioContext_,
                    quint16(record->operands[0] * 256 + record->operands[1]), record->opcode};
                return 1;
            }
            throw Error("Alternate pose audio requires its linked actor state");
        }
        const int key = record->operands[0] * 256 + record->operands[1];
        qsizetype at = 0x212880 + key * 2;
        if (record->opcode == 18 || record->opcode == 20) {
            // The native projector writes a separate output vector. This branch
            // reads the original ROM template's X, not the projected result.
            const float x = std::bit_cast<float>(u32(self.rom_.bytes(), 0x2a8a70));
            if (!std::isfinite(x) || double(x) < -2147483648. || double(x) >= 2147483648.)
                throw Error("Pose audio template is outside the native integer range");
            const int coordinate = int(x);
            if (key >= 0x24e && key < 0x24e + 0xab)
                at += coordinate >= 213 ? 0 : coordinate < 106 ? 4 : 2;
            else
                at = 0x2123a0 + key * 4 + (coordinate < 160 ? 0 : 2);
        }
        if (at < 0x2123a0 || at + 2 > 0x212f00)
            throw Error("Pose audio request exceeds its ROM tables");
        if (self.audio_.size() >= 4096)
            throw Error("Pose audio request budget exceeded");
        self.audio_ << PoseAudioRequest{record->opcode <= 18 ? 0 : 1, u16(self.rom_.bytes(), at)};
        return 1;
    } catch (const Error &e) {
        self.error_ = QString::fromUtf8(e.what());
        return 0;
    }
}
int ActorPosePlayer::advance(int budget) {
    error_.clear();
    if (!invocation_.active) {
        audio_.clear();
        linkedAudio_.clear();
    }
    const int result =
        lordly_actor_advance_pose(&actor_, &invocation_, budget, lookup, shared, this);
    if (result == LORDLY_POSE_BUDGET)
        error_ = "Pose command limit reached; the invocation can resume";
    else if (result == LORDLY_POSE_INVALID)
        error_ = "Pose command produced an invalid field or coordinate";
    return result;
}
int ActorPosePlayer::query() const {
    if (art_.isBody())
        return ActorArt::load(rom_, actor_.bank).poseRecord(actor_.state, actor_.cursor).opcode;
    return art_.poseRecord(actor_.state, actor_.cursor).opcode;
}
bool ActorPosePlayer::bodyBusy() const {
    const auto art = art_.isBody() ? art_
                                   : ActorArt::loadBody(rom_, actor_.bank, actor_.owner,
                                                        actor_.variant, actor_.flag_b);
    const auto opcode = [&](quint32 offset) {
        return art.poseRecord(actor_.state, qint32(quint32(actor_.cursor) + offset)).opcode;
    };
    const int current = opcode(0);
    if (!current)
        return false;
    return current != 1 || opcode(1) != 4 || opcode(2) != 0 || actor_.delay >= 2;
}
} // namespace lordly
