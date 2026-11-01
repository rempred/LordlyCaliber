#include "game/effect_presentation_p04.h"
#include "game/effect_commands_e09.h"
#include "game/effect_handlers_p07.h"
#include "game/effect_math_e04.h"
#include "game/effect_dispatch_e07.h"

typedef struct P07Vec3 { float x, y, z; } P07Vec3;
typedef struct P07Render {
    u8 unknown_00[0xA];
    u16 flags_0A;
    s32 value_0C;
    u8 unknown_10[0xC];
    float value_1C[16];
} P07Render;
typedef struct P07State73EDC {
    s32 *value_00;
    u32 value_04;
    P07Render *value_08, *value_0C, *value_10;
} P07State73EDC;
extern void *func_0020C478(u32);


/* Keep the matrix call together and the subsequent position copy in the caller.
 * This preserves the original call setup without extending its stack-address lifetime. */
static __inline__ void *p07_matrix_55C(void *actor, P07Render *object)
{
    void *destination = object;
    destination = (u8 *)destination + 0x1C;
    func_0026C55C(actor, destination);
    return destination;
}
static __inline__ void *p07_matrix_684(void *actor, P07Render *object)
{
    void *destination = object;
    destination = (u8 *)destination + 0x1C;
    func_0026C684(actor, destination);
    return destination;
}

s32 func_00273EDC(u16 message, P07State73EDC *state)
{
    s32 result = 0;
    P07Vec3 position;
    void *actor;
    void *destination;
    P07Render *object;
    if (message == 0x1000) {
        actor = func_0020C478(state->value_04);
        func_0026C7C4(actor, &position);
        state->value_08 = func_00269798(state->value_00[0]);
        destination = p07_matrix_55C(actor, state->value_08);
        func_0025F150(destination, &position);
        state->value_0C = func_00269798(state->value_00[1]);
        state->value_0C->flags_0A |= 0x1000;
        state->value_0C->value_0C = -1000;
        destination = state->value_0C;
        destination = (u8 *)destination + 0x1C;
        func_0026C684(actor, destination);
        func_0025F150(destination, &position);
        state->value_10 = object = func_0026abc8(state->value_00[2]);
        object->value_1C[0] = position.x;
        object->value_1C[1] = position.y;
        object->value_1C[2] = position.z;
        func_00262830(0x100A);
    } else {
        result = func_00272790(message, (struct E09State72790 *)state);
    }
    return result;
}
