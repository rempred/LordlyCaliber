/* Recovered flat-list lookup: the match POSITION is the physical pose state.
 * Unsigned arithmetic makes the target's 32-bit wrapping explicit on the PC. */
#include "actor_state.h"
int lordly_actor_body_handle(const unsigned short *table, int count, int source,
    int owner, unsigned char source_family, int flag_a, int flag_b, unsigned short *handle)
{
    int base;
    unsigned int index;
    if (!table || !handle || source < 0 || owner < 0 || flag_a < 0 || flag_a > 1 || flag_b < 0 || flag_b > 1)
        return 1;
    base = source_family == owner ? source : owner;
    if (base > 0x1fffffff) return 1;
    index = (unsigned int)base * 4U + (unsigned int)flag_a * 2U + (unsigned int)flag_b;
    if (count < 0 || index >= (unsigned int)count) return 1;
    *handle = table[index];
    return 0;
}
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
