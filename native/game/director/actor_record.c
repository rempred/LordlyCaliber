#include "actor_record.h"
#include <math.h>
#include <string.h>
static uint32_t word(const LordlyActorRecord *r, int at) {
    const uint8_t *b = r->bytes + at;
    return (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
}
static int32_t signed_word(uint32_t v) { return v <= 0x7fffffffU ? (int32_t)v : -1 - (int32_t)~v; }
static int16_t signed_half(uint32_t v) {
    v &= 65535U; return (int16_t)(v < 32768U ? (int)v : (int)v - 65536);
}
static int16_t half(const LordlyActorRecord *r, int at) { return signed_half((uint32_t)r->bytes[at] * 256 + r->bytes[at+1]); }
static void put_word(LordlyActorRecord *r, int at, uint32_t v) {
    r->bytes[at] = (uint8_t)(v >> 24); r->bytes[at+1] = (uint8_t)(v >> 16);
    r->bytes[at+2] = (uint8_t)(v >> 8); r->bytes[at+3] = (uint8_t)v;
}
static void put_half(LordlyActorRecord *r, int at, int v) { r->bytes[at] = (uint8_t)((uint32_t)v >> 8); r->bytes[at+1] = (uint8_t)v; }
static float real(const LordlyActorRecord *r, int at) { uint32_t v = word(r, at); float f; memcpy(&f, &v, 4); return f; }
static void put_real(LordlyActorRecord *r, int at, float f) { uint32_t v; memcpy(&v, &f, 4); put_word(r, at, v); }
void lordly_actor_read_pose(const LordlyActorRecord *r, LordlyActorPose *p) {
    p->bank = signed_word(word(r, 0xe8)); p->owner = signed_word(word(r, 0xec));
    p->cursor = signed_word(word(r, 0xf0)); p->delay = signed_word(word(r, 0xf4)); p->frame = signed_word(word(r, 0xf8));
    p->state = half(r, 0x134); p->previous_state = half(r, 0x136); p->flag_b = half(r, 0x13a);
    p->decoder_mode = r->bytes[0x13d]; p->variant = r->bytes[0x146];
    p->position.x = real(r, 0x11c); p->position.y = real(r, 0x120); p->position.z = real(r, 0x124);
    memcpy(p->material, r->bytes, 16); memcpy(p->material_delta, r->bytes + 16, 16);
}
void lordly_actor_write_pose(LordlyActorRecord *r, const LordlyActorPose *p) {
    put_word(r, 0xe8, (uint32_t)p->bank); put_word(r, 0xec, (uint32_t)p->owner);
    put_word(r, 0xf0, (uint32_t)p->cursor); put_word(r, 0xf4, (uint32_t)p->delay); put_word(r, 0xf8, (uint32_t)p->frame);
    put_half(r, 0x134, p->state); put_half(r, 0x136, p->previous_state); put_half(r, 0x13a, p->flag_b);
    r->bytes[0x13d] = p->decoder_mode; r->bytes[0x146] = p->variant;
    put_real(r, 0x11c, p->position.x); put_real(r, 0x120, p->position.y); put_real(r, 0x124, p->position.z);
    memcpy(r->bytes, p->material, 16); memcpy(r->bytes + 16, p->material_delta, 16);
}
static int finite_position(LordlyActorPosition p) { return isfinite(p.x) && isfinite(p.y) && isfinite(p.z); }
int lordly_actor_body_select(LordlyActorRecord *record, const LordlyActorBodySelection *s,
    int classification, int *advance) {
    LordlyActorRecord next;
    int32_t source, owner, state, flag_b;
    if (!s || !advance || classification < 0 || classification > 2) return LORDLY_ACTOR_INVALID;
    *advance = 0;
    if (s->slot == -1 || !record) return LORDLY_ACTOR_OK;
    if (s->slot < 0 || s->slot >= 28) return LORDLY_ACTOR_INVALID;
    source = s->source == -1 ? signed_word(word(record, 0xe8)) : s->source;
    owner = s->owner == -1 ? signed_word(word(record, 0xec)) : s->owner;
    state = s->state == -1 ? half(record, 0x138) : s->state;
    flag_b = s->flag_b == -1 ? half(record, 0x13a) : s->flag_b;
    if (classification == 2) {
        if (state >= 50) return LORDLY_ACTOR_OK;
        state += 50;
    }
    next = *record;
    put_word(&next, 0xe4, (uint8_t)s->slot);
    if (source != -1) put_word(&next, 0xe8, (uint32_t)source);
    if (owner != -1) put_word(&next, 0xec, (uint32_t)owner);
    if (state != -1) put_half(&next, 0x138, state);
    next.bytes[0x13d] = 1; put_half(&next, 0x134, state);
    put_word(&next, 0xf0, 0xffffffffU); put_word(&next, 0xf4, 0); put_word(&next, 0xf8, 0);
    put_word(&next, 0x128, 0); put_word(&next, 0x12c, 0); put_real(&next, 0x130, 1.f);
    put_word(&next, 0xe0, 0x8022f2ccU); put_half(&next, 0x13a, flag_b);
    next.bytes[0x13c] = s->presentation;
    if (!s->keep_ground) next.bytes[0x145] = 1;
    if (s->flag_a != -1) next.bytes[0x146] = (uint8_t)s->flag_a;
    memset(next.bytes, 255, 16); memset(next.bytes + 16, 0, 16);
    *record = next; *advance = 1;
    return LORDLY_ACTOR_OK;
}
int lordly_actor_select(LordlyActorRecord *record, const LordlyActorSelection *s,
    LordlyActorResolve resolve, void *context, int *advance) {
    LordlyActorRecord next;
    int32_t bank, animation, facing, variant;
    int state;
    if (!record || !s || !resolve || !advance) return LORDLY_ACTOR_INVALID;
    *advance = 0;
    if (s->slot == -1) return LORDLY_ACTOR_OK;
    if (s->slot < 0 || s->slot >= 28 || !finite_position(s->position)) return LORDLY_ACTOR_INVALID;
    bank = s->bank == -1 ? signed_word(word(record, 0xe8)) : s->bank;
    animation = s->animation == -1 ? half(record, 0x138) : s->animation;
    facing = s->facing == -1 ? record->bytes[0x13f] : s->facing;
    variant = s->variant == -1 ? record->bytes[0x146] : s->variant;
    if (!resolve(context, bank, animation, facing, &state)) return LORDLY_ACTOR_RESOURCE;
    if (state == -1) return LORDLY_ACTOR_OK;
    if (!resolve(context, bank, signed_half((uint32_t)animation), facing, &state)) return LORDLY_ACTOR_RESOURCE;
    next = *record;
    put_word(&next, 0xe8, (uint32_t)bank); put_word(&next, 0xe4, (uint32_t)s->slot);
    put_half(&next, 0x138, animation); put_half(&next, 0x134, state);
    next.bytes[0x13f] = next.bytes[0x140] = (uint8_t)facing;
    put_word(&next, 0xf0, 0xffffffffU); put_word(&next, 0xf4, 0); put_word(&next, 0xf8, 0);
    next.bytes[0x146] = (uint8_t)variant; next.bytes[0x13d] = 0;
    /* Native tests Y twice; a Z-only request does not replace coordinates. */
    if (s->position.x != -1.0f || s->position.y != -1.0f) {
        put_real(&next, 0x11c, s->position.x); put_real(&next, 0x120, s->position.y); put_real(&next, 0x124, s->position.z);
    }
    memset(next.bytes, 255, 16);
    *record = next; *advance = 1; return LORDLY_ACTOR_OK;
}
int lordly_actor_construct(LordlyActorRecord *record, const LordlyActorSelection *s, int mode,
    LordlyActorResolve resolve, LordlyActorHeight terrain, void *context) {
    LordlyActorRecord next;
    int state, i;
    float x, z, height;
    if (!record || !s || !resolve || s->slot < 0 || s->slot >= 28 || mode < 0 || mode > 2 || !finite_position(s->position))
        return LORDLY_ACTOR_INVALID;
    memset(&next, 0, sizeof(next));
    if (s->bank != -1) put_word(&next, 0xe8, (uint32_t)s->bank);
    if (s->animation != -1) put_half(&next, 0x138, s->animation);
    if (!resolve(context, signed_word(word(&next, 0xe8)), half(&next, 0x138), s->facing, &state)) return LORDLY_ACTOR_RESOURCE;
    put_half(&next, 0x134, state); put_word(&next, 0xe4, (uint32_t)s->slot);
    put_word(&next, 0xf0, 0xffffffffU); put_real(&next, 0x130, 1.0f);
    for (i = 0; i < 3; ++i) { put_real(&next, 0x104 + i * 4, 1.0f); next.bytes[0x142+i] = 255; }
    next.bytes[0x146] = (uint8_t)s->variant;
    if (s->position.x != -1.0f || s->position.y != -1.0f) {
        put_real(&next, 0x11c, s->position.x); put_real(&next, 0x120, s->position.y); put_real(&next, 0x124, s->position.z);
    }
    put_word(&next, 0xe0, 0x8022f2ccU); // Native draw callback identity; never a host pointer.
    next.bytes[0x13c] = s->presentation;
    next.bytes[0x13f] = next.bytes[0x140] = (uint8_t)s->facing;
    next.bytes[0x141] = s->depth; next.bytes[0x145] = 1;
    if (mode == 0 || mode == 2) {
        if (real(&next, 0x120) == -1.0f) {
            if (!terrain) return LORDLY_ACTOR_TERRAIN;
            x = real(&next, 0x11c); z = real(&next, 0x124);
            if (mode == 2) {
                if ((double)x < -2147483648. || (double)x >= 2147483648. || (double)z < -2147483648. || (double)z >= 2147483648.) return LORDLY_ACTOR_INVALID;
                x = (float)signed_half((uint32_t)(int32_t)x); z = (float)signed_half((uint32_t)(int32_t)z);
            }
            if (!terrain(context, mode, x, z, &height)) return LORDLY_ACTOR_TERRAIN;
            if (!isfinite(height)) return LORDLY_ACTOR_INVALID;
            put_real(&next, 0x120, height);
        } else next.bytes[0x145] = 0;
    }
    next.bytes[0x147] = 255; memset(next.bytes, 255, 16);
    *record = next; return LORDLY_ACTOR_OK;
}
