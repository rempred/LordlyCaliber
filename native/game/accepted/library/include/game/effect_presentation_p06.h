#ifndef GAME_EFFECT_PRESENTATION_P06_H
#define GAME_EFFECT_PRESENTATION_P06_H

#include "common/types.h"
#include "game/combat_draw.h"

/* Partial parameter views follow observed accesses; they do not establish
 * the complete allocation sizes of the records used by these callbacks. */
struct Cd84Header;
struct Ce74InputRecord;
struct P06State56410;
struct P06State56940;
struct P06State56AC8;
struct P06State56F58;
struct P06State59480;
struct P06State59904;
struct P06State73DB0;
struct P06State75850;
struct P06State75AF8;
struct P06IntegerVector75DC4;
struct P06State75E68;
struct P06State75EB0;
struct P06State75EE0;
struct P06State75FF0;
struct P06State762F0;
struct P06State7651C;
struct P06State767BC;
struct P06State76998;
struct P06State76B0C;
struct P06State76CE8;
struct P06State76FE0;
struct P06State77568;
struct P06State77714;
struct P06State778D4;
struct P06State77A40;
struct P06State77CCC;
struct P06State77D60;
struct P06State77E28;
struct P06State77EBC;
struct P06State78560;
struct P06State78B00;
struct P06State78D5C;
struct P06State79290;
struct P06State79460;
struct P06Vec79A90;
struct P06State79A90;

void func_0024CD84(struct Cd84Header *object, u32 flags);
void *func_0024CE74(struct Ce74InputRecord *record);
s32 func_00256410(u16 message, struct P06State56410 *state);
s32 func_00256940(u16 message, struct P06State56940 *state);
s32 func_00256AC8(u16 message, struct P06State56AC8 *state);
s32 func_00256F58(u16 message, struct P06State56F58 *state);
s32 func_00259480(u16 message, struct P06State59480 *state);
s32 func_00259904(u16 message, struct P06State59904 *state);
s32 func_00273DB0(u16 message, struct P06State73DB0 *state);
CombatDrawVertex *func_00275850(struct P06State75850 *state, s16 recordIndex);
void func_00275AF8(struct P06State75AF8 *state, CombatDrawVertex *vertices);
void func_00275DC4(struct P06IntegerVector75DC4 *output, float *first, float *second);
void func_00275E68(struct P06State75E68 *state);
void func_00275EB0(struct P06State75EB0 *state);
s32 func_00275EE0(struct P06State75EE0 *state);
s32 func_00275FF0(u16 message, struct P06State75FF0 *state);
s32 func_002762F0(u16 message, struct P06State762F0 *state);
s32 func_0027651C(u16 message, struct P06State7651C *state);
s32 func_002767bc(u16 message, struct P06State767BC *state);
s32 func_00276998(u16 message, struct P06State76998 *state);
s32 func_00276b0c(u16 message, struct P06State76B0C *state);
s32 func_00276ce8(u16 message, struct P06State76CE8 *state);
s32 func_00276fe0(u16 message, struct P06State76FE0 *state);
s32 func_00277568(u16 message, struct P06State77568 *state);
s32 func_00277714(u16 message, struct P06State77714 *state);
s32 func_002778D4(u16 message, struct P06State778D4 *state);
s32 func_00277A40(u16 message, struct P06State77A40 *state);
s32 func_00277CCC(u16 message, struct P06State77CCC *state);
s32 func_00277D60(u16 message, struct P06State77D60 *state);
s32 func_00277E28(u16 message, struct P06State77E28 *state);
s32 func_00277EBC(u16 message, struct P06State77EBC *state);
s32 func_00278560(u16 message, struct P06State78560 *state);
s32 func_00278B00(u16 message, struct P06State78B00 *state);
s32 func_00278D5C(u16 message, struct P06State78D5C *state);
s32 func_00279290(u16 message, struct P06State79290 *state);
s32 func_00279460(u16 message, struct P06State79460 *state);
s32 func_00279810(u16 message, u8 *state);
s32 func_00279920(u8 value);
u8 func_00279A90(struct P06Vec79A90 *first, struct P06Vec79A90 *second, struct P06State79A90 *state);
u8 func_00279D58(struct P06Vec79A90 *first, struct P06Vec79A90 *second, u8 *state);

#endif
