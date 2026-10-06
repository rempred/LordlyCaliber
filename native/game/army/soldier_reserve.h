#ifndef LORDLY_SOLDIER_RESERVE_H
#define LORDLY_SOLDIER_RESERVE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint16_t lordly_soldier_count(uint16_t hp, uint16_t maximum_hp);
uint16_t lordly_soldier_created_hp(uint16_t maximum_hp, uint8_t count);
int lordly_soldier_free_id(const uint16_t hp[120]);
#ifdef __cplusplus
}
#endif
#endif
