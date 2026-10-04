#ifndef LORDLY_DIALOGUE_MOTION_H
#define LORDLY_DIALOGUE_MOTION_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void lordly_dialogue_move(uint8_t state[1144]);
void lordly_dialogue_bounds(uint8_t window[168], uint8_t state[1144]);
void lordly_dialogue_pointer(const uint8_t window[168], uint8_t state[1144]);
#ifdef __cplusplus
}
#endif
#endif
