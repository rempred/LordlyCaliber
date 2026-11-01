#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "common/types.h"

typedef struct P01Vec3 {
    float x;
    float y;
    float z;
} P01Vec3;

typedef struct P01ExtendedRootState {
    u8 *context;
    u8 *value_04;
    u8 value_08;
} P01ExtendedRootState;

typedef struct P01SixByteRecord {
    u8 bytes[6];
} P01SixByteRecord;

struct P01F730State;
extern s32 func_0024F730(u16, struct P01F730State *);
extern void func_00262B68(void);
extern void func_00262830(s32);

extern void func_00262C64(s16, s16);
extern void *func_00262AB0(void *);




extern void func_002628E4(s32);



extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p01_byte_80220F6D;
extern P01SixByteRecord *p01_pointer_80220F70;
extern P01SixByteRecord p01_records_8020981C[];
extern u8 p01_data_80209816[];
extern u8 p01_data_80209822[];
extern u8 p01_data_80209854[];

s32 func_0024FC0C(u16 message, P01ExtendedRootState *state)
{
    P01Vec3 positions[4];
    u8 *value;
    u8 byte;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Preserve the first shared-context store/read order. */
        *(u8 **)state = p01_context_80220F68;
        value = p01_pointer_80220F6C;
        state->value_04 = value;
        state->value_08 = *value;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->context);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001:
        p01_context_80220F68 = state->context;
        byte = state->value_08;
        p01_byte_80220F6D = 1;
        p01_pointer_80220F70 = p01_records_8020981C;
        *(u8 *)&p01_pointer_80220F6C = byte;
        func_00262AB0((void *)func_0024F730);
        func_0024E9BC(0, state->context[0], 0x33);
        func_00262C64(-1, 0x1002);
        break;
    case 0x1002:
        func_0024DA10(state->value_08, p01_data_80209816);
        func_0024E9BC(0, state->context[0], 0x34);
        func_0025CAF0(state->value_08, 5, 0xA);
        func_00262C64(0x14, 0x1003);
        break;
    case 0x1003:
        func_0024C57C(state->value_08, positions, 0);
        func_0024DFCC(positions, p01_data_80209822, 4);
        func_0025CB60(state->value_04 + 1, 5, 0xA);
        func_0025DAB0(state->value_08, p01_data_80209854);
        func_00262C64(0x14, 0x1004);
        break;
    case 0x1004:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
