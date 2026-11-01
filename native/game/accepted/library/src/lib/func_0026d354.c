#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "game/effect_callbacks_p02.h"

typedef struct P02RootState {
    s16 value_00;
    u16 unknown_02;
    u8 *value_04;
    void *value_08;
    s32 value_0C;
    s32 value_10;
    s32 value_14;
    void *value_18;
    void *value_1C;
    void *value_20;
} P02RootState;

typedef struct P02Parameters {
    s32 value_00;
    float value_04;
    float value_08;
    float value_0C;
    s32 value_10;
} P02Parameters;

extern void func_00262B68(void);
extern void func_00262830(s32);

extern void func_00262C64(s16, s16);

extern s32 func_0026285C(void *, u16);
extern void *func_0020C478(u32);
extern s32 func_0020C014(void *);

extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern void *p01_pointer_80220F70;
extern u8 D_800EB240[];

s32 func_0026d354(u16 message, P02RootState *state)
{
    void *temporary;
    P02Parameters *parameters;
    s32 direction;
    s32 value;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        *(u8 **)((u8 *)state + 4) = p01_context_80220F68;
        *(u8 **)((u8 *)state + 8) = p01_pointer_80220F6C;
        state->value_00 = *(u16 *)&p01_pointer_80220F70;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->value_04);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001:
    case 0x1002:
        /* Keep this parameter address live across the direction query.  Moving
         * its initialization below the call changes the saved-register choice
         * in the pinned compiler, even though the final address setup is later.
         */
        parameters = (P02Parameters *)&p01_context_80220F68;
        direction = func_0024BCE8(state->value_04[0]);
        p01_context_80220F68 = (u8 *)direction;
        if (direction) {
            *(float *)&p01_pointer_80220F6C = 228.0f;
        } else {
            *(float *)&p01_pointer_80220F6C = -228.0f;
        }
        parameters->value_08 = 50.0f;
        parameters->value_0C = 0.0f;
        parameters->value_10 = 1;
        temporary = func_00262AB0((void *)func_0026FBF0);
        func_0026285C(temporary, 0x1007);
        state->value_18 = temporary;
        state->value_20 = func_0026F548(
            func_0020C014(func_0020C478(state->value_04[0])), 20);
        state->value_0C = state->value_10 = state->value_14 = 0;
        value = 0x24;
        switch (state->value_00) {
        case 6: value = 0x25; break;
        case 7: value = 0x26; break;
        }
        func_0024E9BC((s32)D_800EB240, state->value_04[0], value);
        func_00262830(0x1003);
        break;
    case 0x1003:
        ++state->value_0C;
        if (func_0026285C(state->value_18, 0x1005)) {
            func_00262830(0x1004);
        }
        break;
    case 0x1004: {
        u8 byte = state->value_04[0];
        ((u8 *)&p01_context_80220F68)[2] = byte;
        *(u16 *)&p01_context_80220F68 = byte;
        p01_pointer_80220F6C = *(u8 **)((u8 *)state + 8);
        p01_pointer_80220F70 = (void *)func_0026d714;
        state->value_1C = func_00262AB0((void *)func_0026F578);
        func_00262830(0x1005);
        break;
    }
    case 0x1005:
        if (func_0026285C(state->value_1C, 0x1003)) {
            func_0026285C(state->value_1C, 0x101);
            func_0026285C(state->value_18, 0x1008);
            func_00262830(0x1006);
        }
        break;
    case 0x1006:
        ++state->value_10;
        if (!state->value_14 && state->value_10 >= state->value_0C - 20) {
            func_0026285C(state->value_20, 0x1003);
            state->value_14 = 1;
        }
        if (func_0026285C(state->value_18, 0x1006)) {
            func_0026285C(state->value_18, 0x101);
            func_00262830(0x1007);
        }
        break;
    case 0x1007:
        func_00262830(0x101);
        break;
    }
    return 0;
}



