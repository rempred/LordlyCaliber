#ifndef GAME_EFFECT_COMMANDS_E09_H
#define GAME_EFFECT_COMMANDS_E09_H

#include "common/types.h"

/* Numeric access views describe used prefixes, not allocation extents. */
struct E09State700B0;
struct E09State72790;
struct E09StateC6A4;
struct E09StateC8D0;
struct E09StateD3CC;
struct E09StateD6C8;
struct E09StateD8D4;
struct E09StateDC4C;

/* DAB0 and DB00 consume the same halfword/halfword/byte prefix. */
typedef struct Func0025DAB0Input {
    u16 value_00, value_02;
    u8 value_04;
} Func0025DAB0Input;

s32 func_0025C590(s32 value);
s32 func_0025C6A4(u16 command, struct E09StateC6A4 *state);
void func_0025C8A4(void *arg);
s32 func_0025C8D0(u16 command, struct E09StateC8D0 *state);
void func_0025CA7C(u8 value, u8 *input);
void func_0025CAF0(u32 value_arg, u32 value_70, u32 value_71);
void func_0025CB60(u32 source, u8 byte70, u8 byte71);
s32 func_0025D3CC(u16 command, struct E09StateD3CC *state);
void func_0025D678(u8 *input);
s32 func_0025D6C8(u16 command, struct E09StateD6C8 *state);
void func_0025D8A4(s16 *first, s16 *second);
s32 func_0025D8D4(u16 command, struct E09StateD8D4 *state);
void func_0025DAB0(u8 value, const Func0025DAB0Input *input);
void func_0025DB00(void *value, const Func0025DAB0Input *input);
s32 func_0025DB50(u16 command, u8 *state);
s32 func_0025DC4C(u16 command, struct E09StateDC4C *state);
s32 func_002700B0(u16 command, struct E09State700B0 *state);
s32 func_00272790(u16 command, struct E09State72790 *state);

#endif
