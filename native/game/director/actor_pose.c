/* Reconstructed from the retained native Actor contract and original dispatch,
 * not an accepted matching-C source. Host-only bounds are explicit statuses. */
#include "actor_pose.h"
#include <math.h>
#include <string.h>
static int32_t signed_word(uint32_t value) {
    return value <= 0x7fffffffU ? (int32_t)value : -1 - (int32_t)(~value);
}
static int signed_byte(uint8_t value) { return value < 128 ? value : (int)value - 256; }
int lordly_actor_advance_pose_mapped(LordlyActorPose *actor, LordlyPoseInvocation *invocation,
    int budget, LordlyPoseLookup lookup, LordlyPoseShared shared, void *context, LordlyPoseRegistered registered) {
    int i;
    if (!actor || !invocation || !lookup || budget < 0) return LORDLY_POSE_INVALID;
    if (!isfinite(actor->position.x) || !isfinite(actor->position.y) || !isfinite(actor->position.z))
        return LORDLY_POSE_INVALID;
    if (!invocation->active) { invocation->active = 1; invocation->result = 1; }
    while (actor->delay <= 0) {
        LordlyActorPose next;
        LordlyPoseRecord record = {0, {0, 0, 0}};
        uint8_t *bytes;
        if (!budget--) return LORDLY_POSE_BUDGET;
        next = *actor;
        next.cursor = signed_word((uint32_t)next.cursor + 1U);
        if (next.decoder_mode) {
            uint32_t bank = (uint32_t)next.bank & 65535U;
            if ((bank == 135 || bank == 136 || bank == 161 || bank == 65535) &&
                (!registered || !registered(context, &next)))
                next.state = next.state < 50 ? 0 : 50;
        }
        if (!lookup(context, &next, &record)) return LORDLY_POSE_LOOKUP;
        switch (record.opcode) {
        case 0: next.delay = 2; break;
        case 1: next.frame = record.operands[0]; next.delay = record.operands[1]; break;
        case 21:
            next.frame = record.operands[0] + 256 * record.operands[1];
            next.delay = record.operands[2]; break;
        case 2:
            next.position.x -= (float)signed_byte(record.operands[0]);
            next.position.z -= (float)signed_byte(record.operands[1]); break;
        case 3: next.delay = record.operands[0]; break;
        case 4: next.cursor = (int32_t)record.operands[0] - 1; break;
        case 5: case 22:
            i = record.operands[0] + (record.opcode == 22 ? 256 * record.operands[1] : 0);
            if (i > 32767) return LORDLY_POSE_INVALID;
            next.previous_state = next.state; next.state = (int16_t)i;
            next.cursor = -1; break;
        case 12:
            next.position.x += (float)signed_byte(record.operands[0]);
            next.position.y += (float)signed_byte(record.operands[1]);
            next.position.z += (float)signed_byte(record.operands[2]); break;
        case 13: case 16:
            bytes = record.opcode == 13 ? next.material : next.material_delta;
            if (record.operands[0] == 255) memset(bytes, record.operands[1], 16);
            else if (record.operands[0] < 16) bytes[record.operands[0]] = record.operands[1];
            else return LORDLY_POSE_INVALID;
            break;
        case 17: case 18: case 19: case 20:
            if (!shared || !shared(context, &next, &record)) return LORDLY_POSE_SHARED;
            break;
        case 6: case 7: case 8: case 9: case 10: case 11: case 14: case 15: break;
        default: return LORDLY_POSE_INVALID;
        }
        if (!isfinite(next.position.x) || !isfinite(next.position.y) || !isfinite(next.position.z))
            return LORDLY_POSE_INVALID;
        *actor = next; invocation->result = record.opcode;
    }
    for (i = 0; i < 16; ++i) {
        int value = actor->material[i] + signed_byte(actor->material_delta[i]);
        actor->material[i] = (uint8_t)(value < 0 ? 0 : value > 255 ? 255 : value);
    }
    actor->delay = signed_word((uint32_t)actor->delay - 2U);
    invocation->active = 0;
    return LORDLY_POSE_OK;
}

int lordly_actor_advance_pose(LordlyActorPose *actor, LordlyPoseInvocation *invocation,
    int budget, LordlyPoseLookup lookup, LordlyPoseShared shared, void *context) {
    return lordly_actor_advance_pose_mapped(actor, invocation, budget, lookup, shared, context, 0);
}
