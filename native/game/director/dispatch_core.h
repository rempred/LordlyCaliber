#ifndef LORDLY_DIRECTOR_DISPATCH_CORE_H
#define LORDLY_DIRECTOR_DISPATCH_CORE_H
#include "control_flow.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    int words;      /* Full encoded width, including comparison operands. */
    int query;      /* 0: action; 3/4: recovered query record width. */
    int mode;       /* -1: any mode; otherwise the required Director mode. */
} LordlyDirectorDefinition;
typedef struct {
    int resume_cursor; /* D_8022A950, committed only by native control operations. */
    int cursor, nesting, state_scan_mode, handoff, stream_selector, active;
    int mode;
    int executed; /* Work consumed by the most recent bounded dispatch call. */
    LordlyDirectorWord counter; /* D_8022A990, unsigned wrap preserved. */
    unsigned short controller;
    unsigned char barrier_state; /* Optional tracked object byte +0x840; 2 redirects to label 12345. */
} LordlyDirectorDispatch;
enum LordlyDirectorDispatchResult {
    LORDLY_DIRECTOR_YIELD = 0,
    LORDLY_DIRECTOR_END = 1,
    LORDLY_DIRECTOR_SERVICE = 2,
    LORDLY_DIRECTOR_INVALID = 3,
    LORDLY_DIRECTOR_BUDGET = 4,
    LORDLY_DIRECTOR_TRANSFER = 5
};
/* Lookup returns zero for an unknown command. A service returns zero when its
 * external state or implementation is unavailable, without changing host state.
 * Query services return the original producer result, including sentinel -100. */
typedef int (*LordlyDirectorLookup)(void *, LordlyDirectorWord, LordlyDirectorDefinition *);
typedef int (*LordlyDirectorService)(void *, LordlyDirectorWord, const LordlyDirectorWord *, int, int *);
/* Executes the scalar dispatcher control subset recovered in func_00284288.c.
 * Domain operations are supplied by the host. Stream transfers are explicit
 * boundaries; no absent stream or game service is replaced with a successful no-op.
 * active retains local state only for host budget/service suspension, not native
 * yields. The host must not change the stream during such a suspension. */
int lordly_director_dispatch(LordlyDirectorDispatch *state, const LordlyDirectorWord *words,
                            int count, int budget, LordlyDirectorLookup lookup,
                            LordlyDirectorService service, void *context);
#ifdef __cplusplus
}
#endif
#endif
