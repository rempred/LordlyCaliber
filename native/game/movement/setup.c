/* Scenario setup owns rewards, artwork and other features too. Replace only
 * its movement table copy/storm block (US Rev 0 z64 102DB0..102E34).
 * The native copy service takes SOURCE first, DESTINATION second. */
#include "movement_config.h"
extern unsigned char lc_movement_initial[], lc_movement_working[];
extern unsigned char lc_movement_scenario;
extern void lc_movement_copy(const void *, void *, unsigned int);
extern int lc_movement_flag(int);

void lc_movement_setup(void) {
    int row, blocked = 0;
    lc_movement_copy(lc_movement_initial, lc_movement_working, LC_MOVE_ROWS * 64);
    if (lc_movement_scenario == 13)
        blocked = lc_movement_flag(14);
    else if (lc_movement_scenario == 17)
        blocked = lc_movement_flag(15);
    if (blocked)
        for (row = LC_MOVE_ROWS - 1; row >= 0; --row)
            *(unsigned short *)(lc_movement_working + row * 64 + 46) = 0xffff;
}
