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
/* Bounded adaptation of func_002015C8. Retain the high nibble as well as the
 * original low-12-bit descriptor handle, for the separate visual selector. */
int lordly_actor_body_handle(const unsigned short *table, int count, int source,
    int owner, unsigned char source_family, int flag_a, int flag_b, unsigned short *handle);
#ifdef __cplusplus
}
#endif
#endif
