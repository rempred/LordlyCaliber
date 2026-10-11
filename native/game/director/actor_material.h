#ifndef LORDLY_ACTOR_MATERIAL_H
#define LORDLY_ACTOR_MATERIAL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void lordly_tint_begin(uint8_t record[12], const uint32_t args[8]);
void lordly_material_begin(uint8_t record[7], const uint32_t args[7]);
/* Return display-list word count, or -1 for unsafe arithmetic. Null records
 * retain the original absent-record behavior. Caller owns render eligibility. */
int lordly_tint_render(uint8_t *record, uint8_t rgb[3], uint32_t commands[6]);
int lordly_material_render(const uint8_t *record, uint8_t rgb[3], uint32_t commands[14]);
#ifdef __cplusplus
}
#endif
#endif
