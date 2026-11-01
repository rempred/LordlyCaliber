#ifndef GAME_EFFECT_DISPATCH_E07_H
#define GAME_EFFECT_DISPATCH_E07_H

#include "common/types.h"
#include "game/effect_runtime_e03.h"

/* Numeric access views do not establish allocation extents or callback semantics.
 * Public callback parameters carry addresses. Local scratch views preserve the
 * original word/halfword access widths and their measured qualifier contexts. */
struct Func00269470State;
struct Func00269F00Curve;
struct Func00269F00Input;
struct Func00269F00Output;
struct Func0026A078Input;
struct Func0026A078Record;
struct Func0026A204State;
struct Func0026A510Output;
struct Func0026A510Record;
struct Func0026A630Record;
struct Func0026A6E4State;
struct Func0026ADD0Fields;
struct Func0026ADD0State;
struct Func0026B360State;
struct Func0026B820Fields;
struct Func0026B820State;
struct Func0026BE30State;
struct Func0026C09CState;

void *func_00269798(s32 argument);
void *func_002697DC(s32 argument);
void *func_00269834(s32 argument, void *callback);
void *func_002698A0(s32 argument, void *callback);
void *func_0026990C(s32 argument);
void *func_00269964(s32 argument);
void *func_002699BC(s32 argument, u16 value_44, u32 value_48, u16 value_46, u32 value_4C);
void *func_00269A64(s32 argument, u16 value_44, u32 value_48, u16 value_46, u32 value_4C);
void *func_00269B0C(s32 argument, u16 value_44, u32 value_48, u16 value_46, u32 value_4C, void *callback);
void *func_00269BC8(s32 argument, u16 value_44, u32 value_48, u16 value_46, u32 value_4C, void *callback);
void *func_00269C2C(s32 argument, u16 value_44, u32 value_48, u16 value_46, u32 value_4C);
void *func_00269CD4(s32 argument, u16 value_44, u32 value_48, u16 value_46, u32 value_4C);
void *func_00269D7C(s32 argument, u16 value_44, u32 value_48, u16 value_46, u32 value_4C, void *callback);
void *func_00269E38(s32 argument, u16 value_44, u32 value_48, u16 value_46, u32 value_4C, void *callback);
void func_00269F00(s32 argument, struct Func00269F00Input *input, struct Func00269F00Output *output);
void func_0026a078(s32 argument, struct Func0026A078Input *input, struct Func0026A078Record *output);
u32 func_0026a204(u16 command, struct Func0026A204State *state);
void *func_0026a45c(s32 argument);
void *func_0026a494(s32 argument, void *callback);
void *func_0026a4d0(s32 argument);
void func_0026a510(float first, float second, float third, s32 count, struct Func0026A510Record *records, struct Func0026A510Output **slots);
void func_0026a630(float first, float second, float third, s32 count, struct Func0026A630Record *records, void **slots);
u32 func_0026a6e4(u16 command, struct Func0026A6E4State *state);
void *func_0026abc8(s32 argument);
void *func_0026ac0c(s32 argument);
void *func_0026ac64(s32 argument, void *callback);
void *func_0026acd0(s32 argument);
void *func_0026ad28(s32 argument, u16 value_44, u32 value_48, u16 value_46, u32 value_4C);
u32 func_0026add0(u16 command, struct Func0026ADD0State *state);
void func_0026b1fc(struct Func0026ADD0Fields *fields, s32 item);
void func_0026B270(struct Func0026ADD0Fields *fields, s32 item);
void *func_0026B32C(u32 value, u32 count);
u32 func_0026B360(u16 command, struct Func0026B360State *state);
void *func_0026B7E4(u32 value, u32 count);
u32 func_0026B820(u16 command, struct Func0026B820State *state);
void func_0026BCCC(struct Func0026B820Fields *fields, s32 item);
void func_0026BD40(struct Func0026B820Fields *fields, s32 item);
void *func_0026BDFC(u32 value, u32 count);
u32 func_0026BE30(u16 command, struct Func0026BE30State *state);
void *func_0026BF74(s32 argument);
void *func_0026BFB8(s32 argument);
void *func_0026C010(s32 argument);
void *func_0026C070(u32 value);
u32 func_0026C09C(u16 command, struct Func0026C09CState *state);

/* Shared full-entry providers; E06 supplies the dispatcher and curve bodies. */
u32 func_00269470(u32 command, struct Func00269470State *state);

float func_00267990(struct Func00269F00Curve *curve, float value);

#endif
