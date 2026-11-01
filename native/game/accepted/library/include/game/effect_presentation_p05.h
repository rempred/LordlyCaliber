#ifndef GAME_EFFECT_PRESENTATION_P05_H
#define GAME_EFFECT_PRESENTATION_P05_H

#include "common/types.h"
#include "game/effect_adapters_e08.h"

/* Shared P05 numeric views. These prefixes do not establish allocation extents
 * or independent storage for the aliased parameter/table addresses below. */
typedef struct P05Vec3 { float x, y, z; } P05Vec3;
typedef struct P05FactoryRecord {
    u16 value_00;
    u8 selector_02;
    u8 unknown_03;
    u16 flags_04;
} P05FactoryRecord;
typedef P05FactoryRecord P05SixByteRecord;
typedef struct P05BuilderSpec {
    u16 value_00;
    u8 attributes_02;
    u8 unknown_03;
    u16 flags_04;
} P05BuilderSpec;
typedef struct P05BuilderObject {
    u8 unknown_00[10];
    u16 flags_0A;
    s32 value_0C;
    u8 unknown_10[12];
    P05Vec3 position;
    P05Vec3 scale;
} P05BuilderObject;

/* CBA8 writes 1C/1E/20; CD38 restores them from 28/2A/2C.
 * Both functions address the same three primary/secondary pointer pairs. */
typedef struct P05PairChild {
    u8 unknown_00[0x1C];
    u16 value_1C, value_1E, value_20;
    u8 unknown_22[6];
    u16 value_28, value_2A, value_2C;
} P05PairChild;
typedef struct P05PairActor {
    P05PairChild *primary[3];
    P05PairChild *secondary[3];
    u8 unknown_18[0x44];
    s32 value_5C;
} P05PairActor;
typedef struct P05IntegerTriplet { s32 value_00, value_04, value_08; } P05IntegerTriplet;
typedef struct P05State77F90 {
    u8 *context;
    u8 *values;
    u8 unknown_08[2];
    s16 index;
} P05State77F90;

struct P01PlacementObject;
struct P05ScaledObject;
struct P05StepState;
struct P05CountdownState;
struct P05IndexedByteState;
struct P05CoordinateState;
struct P05CopyState;
struct P05ChoreographyState;
struct P05NineEntryState;
struct P05ScanState;
struct P05ThreeStepState;
struct P05StateCDB4;

void func_0024C428(u8 *, u8);
float func_0024C8B4(float *);
s32 func_0024C964(float *, u8, float);
struct P01PlacementObject *func_0024CF10(P05FactoryRecord *);
P05BuilderObject *func_0024D6B8(P05Vec3 *, P05BuilderSpec *, float);
P05BuilderObject *func_0024DA90(u8, P05BuilderSpec *, float);
void func_0024E408(u16);
struct P05ScaledObject *func_0024E430(u16, float);
s32 func_0025145C(u16, struct P05StepState *);
s32 func_002518EC(u16, struct P05CountdownState *);
s32 func_002520C4(u16, void *);
s32 func_00252198(u16, struct P05IndexedByteState *);
s32 func_00257124(u16, struct P05CoordinateState *);
s32 func_00257F54(u16, struct P05CopyState *);
s32 func_002580DC(const void *, const void *);
s32 func_00258178(u16, struct P05ChoreographyState *);
s32 func_00258A28(u16, struct P05NineEntryState *);
s32 func_002590C0(u16, struct P05ScanState *);
s32 func_00259FD4(u16, u8 *);
s32 func_0025A9D0(u16, struct P05ThreeStepState *);
void func_0025CBA8(P05PairActor *, P05IntegerTriplet *);
void func_0025CD38(P05PairActor *);
s32 func_0025CDB4(u16, struct P05StateCDB4 *);
void func_0025D394(u8 *);
s32 func_00277F90(u16, P05State77F90 *);
s32 func_00278278(u16, P05State77F90 *);

/* Existing providers: BF04 consumes flag bits as a word and a vector address;
 * C070 forwards the input word, returning the allocation header address. */

void *func_0026C070(u32);

/* Address/width/qualifier views, not new definitions. The mutable row pointer
 * in CBA8 retains measured reloads; this is not behavioral mutability evidence. */
extern s32 p05_s32_801CEDFC[];
extern u8 p05_byte_80220F6A;
extern s16 p05_halfword_80220F6A;
extern u16 p05_halfword_80220FA8, p05_halfword_80220FAA, p05_halfword_80220FAC;

#endif
