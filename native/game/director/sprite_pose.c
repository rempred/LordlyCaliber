/* Reconstructed from original Rev 0 instructions; not accepted matching C. */
#include "sprite_pose.h"
#include <string.h>
static int32_t signed_word(uint32_t value) {
    return value <= 0x7fffffffU ? (int32_t)value : -1 - (int32_t)(~value);
}
static int signed_byte(uint8_t value) { return value < 128 ? value : (int)value - 256; }
int lordly_sprite_advance_pose(LordlySpritePose *sprite, LordlyPoseInvocation *invocation,
    int budget, LordlySpriteLookup lookup, LordlySpriteSound sound, void *context) {
    if (!sprite || !invocation || !lookup || budget < 0) return LORDLY_POSE_INVALID;
    if (!invocation->active) { invocation->active = 1; invocation->result = 1; }
    while (sprite->delay <= 0) {
        LordlySpritePose next;
        LordlyPoseRecord record = {0, {0, 0, 0}};
        uint8_t *bytes;
        if (!budget--) return LORDLY_POSE_BUDGET;
        next = *sprite;
        next.cursor = signed_word((uint32_t)next.cursor + 1U);
        if (!lookup(context, &next, &record)) return LORDLY_POSE_LOOKUP;
        switch (record.opcode) {
        case 0: next.delay = 2; break;
        case 1: next.frame = record.operands[0]; next.delay = record.operands[1]; break;
        case 21:
            next.frame = record.operands[0] + 256 * record.operands[1];
            next.delay = record.operands[2]; break;
        case 2:
            next.x = signed_word((uint32_t)next.x - (uint32_t)signed_byte(record.operands[0]));
            next.y = signed_word((uint32_t)next.y - (uint32_t)signed_byte(record.operands[1]));
            break;
        case 3: next.delay = record.operands[0]; break;
        case 4: next.cursor = (int32_t)record.operands[0] - 1; break;
        case 5: next.state = record.operands[0]; next.cursor = -1; break;
        case 13: case 16:
            bytes = record.opcode == 13 ? next.color : next.material;
            if (record.operands[0] == 255) memset(bytes, record.operands[1], 16);
            else if (record.operands[0] < 16) bytes[record.operands[0]] = record.operands[1];
            else return LORDLY_POSE_INVALID;
            break;
        case 17: case 18: case 19: case 20:
            if (!sound || !sound(context, &next, &record)) return LORDLY_POSE_SHARED;
            break;
        case 6: case 7: case 8: case 9: case 10: case 11: case 12: case 14: case 15: break;
        default: return LORDLY_POSE_INVALID;
        }
        *sprite = next; invocation->result = record.opcode;
    }
    sprite->delay = signed_word((uint32_t)sprite->delay - 2U);
    invocation->active = 0;
    return LORDLY_POSE_OK;
}
