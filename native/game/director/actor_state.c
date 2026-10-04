/* Recovered flat-list lookup: the match POSITION is the physical pose state.
 * Unsigned arithmetic makes the target's 32-bit wrapping explicit on the PC. */
#include "actor_state.h"
int lordly_actor_resolve_state(const short *table, int count, int bank, int animation,
                               int facing, int option, int *state)
{
    int first, index;
    LordlyDirectorWord target = (LordlyDirectorWord)animation * 10U + (LordlyDirectorWord)facing;
    if (!table || !state || count < 1 || bank < 1 || bank > 16384 || (bank - 1) * 2 >= count)
        return 1;
    first = table[(bank - 1) * 2];
    if (first < 0 || first >= count) return 1;
    for (index = 0; index <= 32767 && first + index < count; ++index) {
        const int current = table[first + index];
        if (current == -1) { *state = -1; return 0; }
        if ((LordlyDirectorWord)current == target) { *state = option ? -1 : index; return 0; }
    }
    return 1;
}
