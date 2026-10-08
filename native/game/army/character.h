#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint32_t (*LordlyCharacterRandom)(void *);
/* Accepted resident initialization and growth, ROM4501C/44AA4. Caller owns
 * allocation, names and IDs. Tables contain full 72-byte class / 32-byte item
 * records (no item-table prefix). RNG returns nonnegative 31-bit values. */
int lordly_character_initialize(uint8_t record[56], unsigned level, const uint8_t *classes,
                                unsigned count, const uint8_t *items, unsigned item_count,
                                LordlyCharacterRandom random, void *context);
int lordly_character_level_one(uint8_t record[56], const uint8_t *classes, unsigned count,
                               LordlyCharacterRandom random, void *context);
#ifdef __cplusplus
}
#endif
