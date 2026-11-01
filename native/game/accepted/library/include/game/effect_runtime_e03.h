#ifndef GAME_EFFECT_RUNTIME_E03_H
#define GAME_EFFECT_RUNTIME_E03_H

#include "common/types.h"

/* Address arguments and results are opaque at shared boundaries. The runtime
 * assigns the observed pool/task view internally; callers retain their own
 * payload prefixes. Callback words are stored and invoked with the original
 * command and record/payload addresses. Numeric names retain uncertain meaning. */
struct E03Task;

void func_00261C90(void);
void func_00261D00(void);
void *func_00261D24(void);
void func_00261D7C(void *payload);
void *func_00261DA8(void *value);
void func_00261E30(void *payload);
void func_00261E90(void);
void func_00261F60(void);
void func_00261F84(void);
void *func_00262264(void);
void func_002622B4(void *recordView);
void *func_00262328(void *callback);
void func_00262434(void *recordView);
void func_00262450(void);
void func_00262588(void);
void func_002625AC(void);
void *func_00262694(void);
void func_002626E4(void *taskView);
void func_00262730(struct E03Task *after, struct E03Task *task);
void func_002627E0(struct E03Task *task);
s32 func_0026285C(void *taskView, u16 command);
void *func_00262910(void *callback, void *afterView);
void *func_00262AB0(void *callback);
void func_00262AD4(struct E03Task *task);
void func_00262B68(void);
void *func_00262C04(void *parentView, void *callback);
void func_00262C80(void);
void func_00262C94(void *recordView, s32 value, s32 resume_state);
void func_00262CC0(void);
void func_00262DA4(void);
void func_00262DD4(u16 value);
void func_00262F50(u16 value);
void func_00263050(void);
void *func_00263074(void);
void func_002630CC(void *recordView);
void *func_00263120(void *callback, s32 value);

#endif
