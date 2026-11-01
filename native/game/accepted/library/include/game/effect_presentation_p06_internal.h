#ifndef GAME_EFFECT_PRESENTATION_P06_INTERNAL_H
#define GAME_EFFECT_PRESENTATION_P06_INTERNAL_H

#include "game/effect_presentation_p06.h"
#include "game/effect_runtime_e03.h"
#include "game/effect_math_e04.h"
#include "game/effect_adapters_e08.h"
#include "game/effect_commands_e09.h"
#include "game/effect_dispatch_e07.h"

/* These three scalar components are the common local vector shape. */
typedef struct P06Vec3 { float x, y, z; } P06Vec3;

/* The 80220F68/80220F6C globals retain separately named word, byte,
 * halfword, pointer and matrix access views in their actual message phases.
 * Shared addresses do not imply a single recovered semantic record type. */

/* Signatures follow the accepted providers, including narrow message values. */
void func_00262830(s16 value);
void func_00262C64(s16 count, s16 command);
void func_002628E4(s32 value);
void func_0026241C(void *object, s32 first, s32 second);
void func_002623C4(void *object);
void *func_00261560(s32 value);
void func_00260C24(void *data);
void func_0024C7A4(void *output, void *first, void *second);
void *func_0020C478(u32 index);
s32 func_0020C014(void *object);
void *resource_alloc(u32 bytes);
s32 resource_free(u8 *pointer);

/* Unchanged KEEP providers in this complete wave. */
s32 func_0024BD60(s32 value);
void func_00275D8C(void *state, s32 value);
void func_00275DA0(void *state, float value);
void func_00275DBC(void *state, s16 value);
void func_00275E3C(void *state);
void func_00275E4C(void *state);
s32 func_00279A50(void *state);

#endif
