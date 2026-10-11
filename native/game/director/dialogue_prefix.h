#ifndef LORDLY_DIALOGUE_PREFIX_H
#define LORDLY_DIALOGUE_PREFIX_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* ROM EA930..EACC0 at RAM 8019DBB0. Stops before the first body control.
 * Returns -1 on a source overrun. The host stages buffers for atomic failure. */
int lordly_dialogue_prefix(uint8_t window[168], uint8_t state[1144], const uint8_t *source,
                           size_t length, uint32_t *cursor);
#ifdef __cplusplus
}
#endif
#endif
