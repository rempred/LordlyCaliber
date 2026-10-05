/* Host adaptation of accepted func_001F0E64 / func_001F1218 C at Decomp
 * main 62cc7ef4. Direct descriptor route (arg1=0x100). Explicit byte storage,
 * bounded stacks and resumable callbacks replace native pointers/services.
 * This host adaptation is not itself accepted matching C. */
#include "direct_pose.h"
#include <string.h>
static unsigned half(const uint8_t *p, int at) { return p[at] * 256U + p[at + 1]; }
static int shalf(const uint8_t *p, int at) {
    unsigned x = half(p, at); return x < 32768 ? (int)x : (int)x - 65536;
}
static void put(uint8_t *p, int at, unsigned x) { p[at] = (uint8_t)(x >> 8); p[at+1] = (uint8_t)x; }
static int sbyte(uint8_t x) { return x < 128 ? x : (int)x - 256; }
static uint32_t selection(LordlyDirectRandom random, void *context) {
    uint32_t first = random(context), second = random(context), third = random(context);
    return (((first << 18) & 0x0c000000U) | (second << 15) | third) % 5U;
}
int lordly_direct_initialize(uint8_t *p, uint32_t selector, LordlyDirectRandom random, void *context) {
    if (!p) return LORDLY_POSE_INVALID;
    if ((uint8_t)selector == 255) return LORDLY_POSE_OK;
    if (!selector && (p[0x48] & 2)) {
        if (!random) return LORDLY_POSE_SHARED;
        selector = selection(random, context);
    }
    if (p[0x48] & 1) selector += 50;
    put(p, 4, selector); put(p, 6, 65535); put(p, 8, 0);
    put(p, 12, 0); put(p, 14, 0); put(p, 16, 0);
    p[0x46] = p[0x47] = 0;
    memset(p + 0x12, 255, 16); memset(p + 0x22, 0, 16);
    return LORDLY_POSE_OK;
}
int lordly_direct_advance(uint8_t *p, LordlyPoseInvocation *invocation, int budget,
                         LordlyDirectLookup lookup, LordlyDirectSound sound,
                         LordlyDirectRandom random, void *context) {
    if (!p || !invocation || !lookup || budget < 0) return LORDLY_POSE_INVALID;
    if (!invocation->active) { invocation->active = 1; invocation->result = 0; }
    while (shalf(p, 8) <= 0) {
        uint8_t next[80], *bytes;
        LordlyPoseRecord r = {0, {0, 0, 0}};
        unsigned depth;
        if (!budget--) return LORDLY_POSE_BUDGET;
        memcpy(next, p, 80); put(next, 6, half(next, 6) + 1);
        if (!lookup(context, next, &r)) return LORDLY_POSE_LOOKUP;
        switch (r.opcode) {
        case 0:
            depth = next[0x46];
            if (depth > 4) return LORDLY_POSE_INVALID;
            if (depth) {
                next[0x46] = (uint8_t)--depth;
                put(next, 4, next[0x32 + depth]); put(next, 6, next[0x36 + depth]);
            } else {
                int result = lordly_direct_initialize(next, 0, random, context);
                if (result != LORDLY_POSE_OK) return result;
            }
            break;
        case 1: put(next, 10, r.operands[0]); put(next, 8, r.operands[1]); break;
        case 21:
            put(next, 10, r.operands[0] + 256U * r.operands[1]);
            put(next, 8, r.operands[2]); break;
        case 3: put(next, 8, r.operands[0]); break;
        case 2:
            put(next, 16, half(next, 16) - sbyte(r.operands[0]));
            put(next, 14, half(next, 14) - sbyte(r.operands[1])); break;
        case 12:
            put(next, 16, half(next, 16) - sbyte(r.operands[0]));
            put(next, 14, half(next, 14) + sbyte(r.operands[1]));
            put(next, 12, half(next, 12) + sbyte(r.operands[2])); break;
        case 13: case 16:
            bytes = next + (r.opcode == 13 ? 0x12 : 0x22);
            if (r.operands[0] == 255) memset(bytes, r.operands[1], 16);
            else if (r.operands[0] < 16) bytes[r.operands[0]] = r.operands[1];
            else return LORDLY_POSE_INVALID;
            break;
        case 15:
            depth = next[0x46];
            if (depth >= 4) return LORDLY_POSE_INVALID;
            next[0x32 + depth] = (uint8_t)half(next, 4);
            next[0x36 + depth] = (uint8_t)half(next, 6);
            next[0x46]++;
            /* fall through */
        case 5: put(next, 6, 65535); put(next, 4, r.operands[0]); break;
        case 14:
            depth = next[0x47];
            if (depth > 4) return LORDLY_POSE_INVALID;
            if (depth && next[0x39 + depth] == shalf(next, 4) &&
                next[0x3d + depth] == shalf(next, 6)) {
                if (!--next[0x41 + depth]) { next[0x47]--; break; }
            } else {
                if (!r.operands[1]) break;
                if (depth == 4) return LORDLY_POSE_INVALID;
                next[0x3a + depth] = (uint8_t)half(next, 4);
                next[0x3e + depth] = (uint8_t)half(next, 6);
                next[0x42 + depth] = r.operands[1]; next[0x47]++;
            }
            /* fall through */
        case 4: put(next, 6, (unsigned)r.operands[0] - 1); break;
        case 17: case 18: case 19: case 20:
            if (!sound || !sound(context, next, &r)) return LORDLY_POSE_SHARED;
            break;
        default: break;
        }
        memcpy(p, next, 80); invocation->result = r.opcode;
    }
    for (int i = 0; i < 16; ++i) {
        const int sum = p[0x12 + i] + sbyte(p[0x22 + i]);
        p[0x12 + i] = (uint8_t)(sum < 0 ? 0 : sum > 255 ? 255 : sum);
    }
    put(p, 8, half(p, 8) - 2); invocation->active = 0;
    return LORDLY_POSE_OK;
}
