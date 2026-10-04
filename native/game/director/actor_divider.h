#ifndef LORDLY_ACTOR_DIVIDER_H
#define LORDLY_ACTOR_DIVIDER_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void lordly_actor_divider_begin(uint8_t record[12], const uint32_t args[6]);
uint8_t lordly_actor_divider_channel(const uint8_t record[12], float x, float z);
#ifdef __cplusplus
}
#endif
#endif
