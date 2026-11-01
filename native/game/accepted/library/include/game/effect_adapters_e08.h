#ifndef GAME_EFFECT_ADAPTERS_E08_H
#define GAME_EFFECT_ADAPTERS_E08_H

#include "common/types.h"

/* Numeric partial views describe observed accesses, not allocation extents. */
struct B4a0State;
struct B5f4State;
struct B714State;
struct B7b8State;
struct B85cState;
struct B954State;
struct Bad4State;
struct BbccState;
struct BcdcState;
struct Be00State;
struct Bef8State;
struct C0e4State;
struct C1e8State;
struct C2e8State;
struct C3dcState;
struct Dc20InputRecord;
struct P01PlacementObject;
struct P01Vec3;

u32 func_0024BCA0(u32 argument);
s16 func_0024BCE8(u8 value);
void func_0024BF04(u8 value, struct P01Vec3 *output, s32 flags);
void func_0024C0DC(u8 value, struct P01Vec3 *output, float xOffset, float yOffset, float zOffset);
void func_0024C204(u8 *values, struct P01Vec3 *output, s32 flags);
s16 func_0024C2F8(struct P01Vec3 *position);
s16 func_0024C3EC(u8 value);
void func_0024C57C(u8 value, struct P01Vec3 *output, s32 mode);
void func_0024C7D8(struct P01Vec3 *input, struct P01PlacementObject *object);
void func_0024CD40(void *stateView);
void *func_0024CDD8(void *recordView);
void *func_0024D180(void *vectorView, void *recordView);
void *func_0024D41C(void *vectorView, void *recordView);
void *func_0024DA10(u32 value, void *argument);
void func_0024DA50(u8 value, void *argument);
void *func_0024DAE4(void *vectorView, void *recordView, s32 index);
void *func_0024DC20(u8 value, struct Dc20InputRecord *record, s32 index);
void *func_0024DFCC(struct P01Vec3 *vectors, void *recordView, s32 count);
void func_0024E490(u32 value, const u8 *records, u32 count);
void *func_0024E510(void *vectorView, void *recordView);
void func_0024E7AC(u8 value, u32 flags);
void func_0024E898(u8 value, s32 argument);
void func_0024E9BC(s32 argument, u8 index, s32 value);
void func_0025B4A0(u16 command, struct B4a0State *state);
void func_0025B5F4(u16 command, struct B5f4State *state);
void func_0025B714(u16 command, struct B714State *state);
void func_0025B7B8(u16 command, struct B7b8State *state);
void func_0025B85C(u16 command, struct B85cState *state);
void func_0025B954(u16 command, struct B954State *state);
void func_0025BAD4(u16 command, struct Bad4State *state);
void func_0025BBCC(u16 command, struct BbccState *state);
void func_0025BCDC(u16 command, struct BcdcState *state);
void func_0025BE00(u16 command, struct Be00State *state);
void func_0025BEF8(u16 command, struct Bef8State *state);
void func_0025C0E4(u16 command, struct C0e4State *state);
void func_0025C1E8(u16 command, struct C1e8State *state);
void func_0025C2E8(u16 command, struct C2e8State *state);
void func_0025C3DC(u16 command, struct C3dcState *state);

/* Original descriptor 11 table at ROM 0x25E2F4, shared with CF10.
 * Entries are the accepted E07 pointer-returning factories. */
extern void *(*D_80209734[])(s32);

#endif
