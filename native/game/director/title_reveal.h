#ifndef LORDLY_TITLE_REVEAL_H
#define LORDLY_TITLE_REVEAL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void lordly_title_start(uint8_t record[14], const uint32_t args[6]);
/* Zero releases the owner; progress advances here, never during drawing. */
int lordly_title_update(uint8_t record[14]);
/* Two E4 rectangle words, texture origin and derivatives per rectangle.
 * Returns zero for the original division-by-zero drawing fault. */
int lordly_title_rectangles(const uint8_t record[14], int high_resolution, uint32_t words[8]);
#ifdef __cplusplus
}
#endif
#endif
