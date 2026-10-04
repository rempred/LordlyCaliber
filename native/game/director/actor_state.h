#ifndef LORDLY_ACTOR_STATE_H
#define LORDLY_ACTOR_STATE_H
#include "control_flow.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Bounded PC adaptation of func_002A13EC. A normal no-match returns state -1;
 * status 1 means the native pointer walk would exceed the supplied table. */
int lordly_actor_resolve_state(const short *table, int count, int bank, int animation,
                               int facing, int option, int *state);
#ifdef __cplusplus
}
#endif
#endif
