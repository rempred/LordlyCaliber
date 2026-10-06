#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint32_t (*LordlyCharacterRandom)(void *);
/* Level-one branch of resident ROM4501C..453E0. The caller owns allocation,
 * names and IDs. Tables contain full 72-byte records, including their header. */
int lordly_character_level_one(uint8_t record[56], const uint8_t *classes, unsigned count,
                               LordlyCharacterRandom random, void *context);
#ifdef __cplusplus
}
#endif
