#ifndef GAME_EFFECT_CALLBACKS_P02_H
#define GAME_EFFECT_CALLBACKS_P02_H

#include "common/types.h"

/* Callbacks passed to func_00262AB0 receive its task payload, 0x24 bytes
 * past the factory return value. Other callbacks retain their own state views.
 * Numeric names and local field views retain uncertain meanings. */
struct P02ChildState;
struct P02RootState;
struct P02State2705EC;
struct P02State2711DC;
struct P02State271D18;
struct P02State271E5C;
struct P02State27267C;
struct P02StateD670;
struct P02StateD918;
struct P02StateDCF0;
struct P02StateE520;
struct P02StateE64C;
struct P02StateF12C;
struct P02StateF1E0;
struct P02StateF578;
struct P02StateF7F0;
struct P02StateFBF0;

/* Both entry callbacks explicitly return a word. Their shared physical
 * entry/state layout is used by F7F0 and 70F10. */
typedef struct P02EntryF7F0 P02EntryF7F0;
struct P02EntryF7F0 {
    s32 (*value_00)(P02EntryF7F0 *);
    u8 unknown_04[0x2C];
    void *value_30, *value_34;
    u16 value_38, value_3A, value_3C, value_3E;
    u16 value_40, value_42, value_44, value_46;
};

typedef struct P02HeaderF7F0 {
    u32 unknown_00, value_04;
} P02HeaderF7F0;

typedef struct P02StateF7F0 {
    u8 unknown_00[0x50];
    P02HeaderF7F0 *value_50;
    u32 unknown_54;
    P02EntryF7F0 **value_58;
} P02StateF7F0;

/* Shared parameter symbols name typed address views of the same scratch
 * storage used by P01; they do not establish independent object extents. The
 * linked effective addresses and each access width are checked against retail. */

/* Current canonical providers return a scalar word; callers choose the
 * explicit address view where retail stores that word as a pointer. */
void *func_00262AB0(void *callback);
struct SupplementBoundsRecord;
s32 func_00200474(void);
s32 func_0020D590(struct SupplementBoundsRecord *record);

/* The helper loads an unsigned byte, but D770 and F990 pass its return word
 * directly. A byte return type adds caller-side masking with the pinned compiler. */
u32 func_0025F73C(s32 index);

void func_0026cd1c(void *stateView);
s32 func_0026ce74(u16 message, struct P02ChildState *state);
s32 func_0026d354(u16 message, struct P02RootState *state);
s32 func_0026d670(u16 message, struct P02StateD670 *state);
void *func_0026d714(s16 value, u32 first, u32 second, s32 unusedOrdinal);
void func_0026d770(void *stateView);
s32 func_0026d918(u16 message, struct P02StateD918 *state);
s32 func_0026dcf0(u16 message, struct P02StateDCF0 *state);
s32 func_0026e20c(u16 message, struct P02RootState *state);
s32 func_0026e520(u16 message, struct P02StateE520 *state);
void *func_0026e5c8(s16 value, u32 first, u32 second, s32 unusedOrdinal);
s32 func_0026e64c(u16 message, struct P02StateE64C *state);
void *func_0026edc0(s16 value, u32 first, u32 second, s32 unusedOrdinal);
s32 func_0026ee18(u16 message, struct P02RootState *state);
s32 func_0026F12C(u16 message, struct P02StateF12C *state);
s32 func_0026F1E0(u16 message, struct P02StateF1E0 *state);
void *func_0026F548(s32 first, s32 second);
s32 func_0026F578(u16 message, struct P02StateF578 *state);
void func_0026F7F0(u16 message, struct P02StateF7F0 *state);
void func_0026F990(void *stateView);
s32 func_0026FBF0(u16 message, struct P02StateFBF0 *state);
void func_002702A0(void *stateView);
void func_00270488(void *stateView);
s32 func_002705EC(u16 message, struct P02State2705EC *state);
s32 func_00270BF4(u16 message, struct P02RootState *state);
void func_00270F10(u16 message, struct P02StateF7F0 *state);
void *func_00270FF0(s16 value, u32 first, u32 second, s32 ordinal);
void func_0027109C(void *stateView);
s32 func_002711DC(u16 message, struct P02State2711DC *state);
s32 func_00271A10(u16 message, struct P02RootState *state);
s32 func_00271D18(u16 message, struct P02State271D18 *state);
void *func_00271DBC(s16 value, u32 first, u32 second, s32 ordinal);
s32 func_00271E5C(u16 message, struct P02State271E5C *state);
s32 func_00272374(u16 message, struct P02RootState *state);
s32 func_0027267C(u16 message, struct P02State27267C *state);
void *func_0027271C(s16 value, u32 first, u32 second, s32 ordinal);

#endif
