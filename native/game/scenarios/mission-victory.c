/* Authored maps without enemy squads win by liberating their declared enemy
 * stronghold. Ordinary missions retain the native boss/army predicate.
 * Site flags and local indexing are from accepted func_00127830 and
 * func_00108AA0_group; no persistent story flags are repurposed. */
#include "mission-victory-config.h"
typedef unsigned char u8;
typedef unsigned short u16;
extern int lc_native_mission_victory(void);
static const u8 mission_rule[] = LC_OBJECTIVES;
int lc_authored_mission_victory(void) {
    const unsigned key = *(volatile u8 *)0x8018f481;
    const unsigned objective = key < sizeof(mission_rule) ? mission_rule[key] : 0;
    int result;
    if (objective & 128) {
        *(volatile u8 *)0x8018f481 = objective & 127;
        result = lc_native_mission_victory();
        *(volatile u8 *)0x8018f481 = key;
        return result;
    }
    if (!objective) return lc_native_mission_victory();
    if (objective > *(volatile unsigned *)0x801f1070) return 0;
    return (*(volatile u16 *)(0x801951cc + (objective - 1) * 36) & 6) == 2;
}
