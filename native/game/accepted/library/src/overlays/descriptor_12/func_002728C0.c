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
typedef struct P07State728C0 {
    u32 value_00, value_04;
    P07Render *value_08[3];
    P07Render *value_14, *value_18, *value_1C;
} P07State728C0;

extern void *func_0020C478(u32);


/* Numeric view of the three original words at ROM 0x275650. */
extern s32 p07_data_80220AA0[];

/* Keeping these complete calls inline preserves retail argument setup with
 * the pinned compiler. The following position copy stays in the caller so
 * its stack address does not gain a separate call-crossing lifetime. */
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

s32 func_002728C0(u16 message, P07State728C0 *state)
{
    s32 result = 0;
    P07Vec3 position;
    u32 i;
    void *actor;
    P07Render *object;
    void *destination;

    if (message == 0x1000) {
        actor = func_0020C478(state->value_04);
        i = 0;
        func_0026C7C4(actor, &position);
        do {
            state->value_08[i] = object = func_0026abc8(p07_data_80220AA0[i]);
            object->value_1C[0] = position.x;
            object->value_1C[1] = position.y;
            object->value_1C[2] = position.z;
        } while (++i < 3);
        state->value_14 = func_00269798(0x183);
        state->value_14->flags_0A |= 0x1000;
        state->value_14->value_0C = -1000;
        destination = state->value_14;
        destination = (u8 *)destination + 0x1C;
        func_0026C684(actor, destination);
        func_0025F150(destination, &position);
        state->value_18 = func_00269798(0x184);
        destination = p07_matrix_684(actor, state->value_18);
        func_0025F150(destination, &position);
        state->value_1C = func_00269798(0x185);
        destination = p07_matrix_55C(actor, state->value_1C);
        func_0025F150(destination, &position);
        func_00262830(0x100A);
    } else {
        result = func_00272790(message, (struct E09State72790 *)state);
    }
    return result;
}
