#include "game/effect_presentation_p04.h"
#include "game/effect_commands_e09.h"
#include "game/effect_dispatch_e07.h"
#include "game/effect_handlers_p07.h"
#include "game/effect_math_e04.h"

typedef struct P07Vec3 { float x, y, z; } P07Vec3;
typedef struct P07Render {
    u8 unknown_00[0xA];
    u16 flags_0A;
    s32 value_0C;
    u8 unknown_10[0xC];
    float value_1C[16];
} P07Render;
typedef struct P07State72A2C {
    u32 value_00, value_04;
    P07Render *value_08, *value_0C, *value_10;
} P07State72A2C;
extern void *func_0020C478(u32);


static __inline__ void *p07_matrix_55C(void *actor, P07Render *object)
{
    void *destination = object;
    destination = (u8 *)destination + 0x1C;
    func_0026C55C(actor, destination);
    return destination;
}

/* This complete phase retains the position address across its two calls. */
static __inline__ void p07_phase_1001(P07State72A2C *state, void *actor, P07Vec3 *position)
{
    void *destination;
    state->value_0C = func_00269798(0x19C);
    state->value_0C->flags_0A |= 0x1000;
    state->value_0C->value_0C = -1000;
    destination = state->value_0C;
    destination = (u8 *)destination + 0x1C;
    func_0026C684(actor, destination);
    func_0025F150(destination, position);
    func_00262830(0x100A);
}

s32 func_00272A2C(u16 message, P07State72A2C *state)
{
    s32 result = 0;
    switch (message) {
    case 0x1000: {
        P07Vec3 position;
        void *actor;
        void *destination;
        P07Render *object;
        actor = func_0020C478(state->value_04);
        func_0026C7C4(actor, &position);
        state->value_08 = func_00269798(0x19A);
        destination = p07_matrix_55C(actor, state->value_08);
        func_0025F150(destination, &position);
        state->value_10 = object = func_0026abc8(0x19B);
        object->value_1C[0] = position.x;
        object->value_1C[1] = position.y;
        object->value_1C[2] = position.z;
        func_00262C64(10, 0x1001);
        break;
    }
    case 0x1001: {
        P07Vec3 position;
        void *actor = func_0020C478(state->value_04);
        func_0026C7C4(actor, &position);
        p07_phase_1001(state, actor, &position);
        break;
    }
    default:
        result = func_00272790(message, (struct E09State72790 *)state);
        break;
    }
    return result;
}
