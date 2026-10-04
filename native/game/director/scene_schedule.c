/* Portable control adaptation of ROM 2A0680..2A0804 (Actor overlay RAM
 * 80230E50). Reference func_002A0680.c at Decomp
 * b5ee7cb365ebb4abb6c59952675669f9a9c30fa4 is HYBRID_C, not accepted pure C.
 * This implementation contains no assembler. Services own domain state. */
#include "scene_schedule.h"

uint32_t lordly_scene_service_address(LordlySceneService service) {
    static const uint32_t addresses[LORDLY_SCENE_SERVICE_COUNT] = {
        0x8022CBD4U, 0x8023C71CU, 0x8023EAD0U, 0x8023A7F4U, 0x802401B4U, 0x80241610U,
        0x8024049CU, 0x8023BDDCU, 0x80227694U, 0x8022AE84U, 0x802316C0U, 0x8022CF60U,
        0x80230FD4U, 0x8023E354U, 0x801C3D64U, 0x80239AF8U, 0x8023DAC4U, 0x802282B8U,
        0x80230B60U, 0x802345E0U, 0x80234770U, 0x80178E30U};
    return service >= 0 && service < LORDLY_SCENE_SERVICE_COUNT ? addresses[service] : 0;
}

int lordly_scene_schedule(LordlySceneSchedule *s, LordlySceneInvoke invoke, void *context,
                          int budget) {
    LordlySceneService service;
    int next, value;
    if (!s || budget < 1 || (s->active && (s->phase < 0 || s->phase > 30)))
        return LORDLY_SCENE_INVALID;
    s->executed = 0;
    if (!s->active) {
        s->phase = s->alternate ? 28 : 0;
        s->active = 1;
    }
    for (;;) {
        next = s->phase + 1;
        switch (s->phase) {
        case 0:
            service = LORDLY_SCENE_COLOR;
            break;
        case 1:
            s->phase = s->mode == 0 ? 2 : 8;
            continue;
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
            service = (LordlySceneService)(LORDLY_SCENE_MODE_ZERO_1 + s->phase - 2);
            break;
        case 8:
            s->phase = s->mode == 2 ? 9 : 10;
            continue;
        case 9:
            service = LORDLY_SCENE_DIVIDER;
            break;
        case 10:
            service = LORDLY_SCENE_COUNTER;
            break;
        case 11:
            service = LORDLY_SCENE_HEIGHT;
            break;
        case 12:
            service = LORDLY_SCENE_MOVEMENT;
            break;
        case 13:
            service = LORDLY_SCENE_FACING;
            break;
        case 14:
            service = LORDLY_SCENE_POSES;
            break;
        case 15:
            s->phase = s->mode == 2 ? 16 : 20;
            continue;
        case 16:
            service = LORDLY_SCENE_MODE_TWO_1;
            break;
        case 17:
            service = LORDLY_SCENE_MODE_TWO_2;
            break;
        case 18:
            service = LORDLY_SCENE_PROJECTION;
            break;
        case 19:
            service = LORDLY_SCENE_MODE_TWO_4;
            break;
        case 20:
            service = LORDLY_SCENE_DIRECTOR;
            break;
        case 21:
            service = LORDLY_SCENE_AFTER_DIRECTOR;
            break;
        case 22:
            service = LORDLY_SCENE_PATHS;
            break;
        case 23:
            service = LORDLY_SCENE_TRANSITION;
            break;
        case 24:
            value = s->delayed_counter < 128 ? s->delayed_counter : s->delayed_counter - 256;
            if (value == 0)
                value = s->delayed_counter = 10;
            if (value >= 10) {
                s->delayed_counter = (uint8_t)(value + 1);
                value = s->delayed_counter < 128 ? s->delayed_counter : s->delayed_counter - 256;
            }
            s->phase = value >= 41 ? 25 : 27;
            continue;
        case 25:
            service = LORDLY_SCENE_DELAYED_CALLBACK;
            break;
        case 26:
            s->delayed_counter = 255;
            s->phase = 27;
            continue;
        case 27:
        case 30:
            s->active = 0;
            s->phase = 0;
            return LORDLY_SCENE_COMPLETE;
        case 28:
            service = LORDLY_SCENE_COUNTER;
            break;
        case 29:
            service = LORDLY_SCENE_DIRECTOR;
            break;
        default:
            return LORDLY_SCENE_INVALID;
        }
        if (s->executed == budget)
            return LORDLY_SCENE_BUDGET;
        if (!invoke || !invoke(context, service))
            return LORDLY_SCENE_UNAVAILABLE;
        ++s->executed;
        s->phase = next;
    }
}
