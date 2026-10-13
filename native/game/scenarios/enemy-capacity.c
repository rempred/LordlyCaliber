#include "enemy-capacity.h"

extern unsigned short lc_enemy_soldier_max_hp;
extern void lc_capacity_clear(void *data, int size);
extern void lc_capacity_character(int index, const unsigned char *spec, int selector,
                                  unsigned char flag, unsigned char source);
extern unsigned char lc_capacity_finalize(unsigned char *source);
extern void lc_capacity_player_construct(const unsigned char *spec, int source);

int lc_enemy_is_soldier(unsigned int handle)
{
    return handle >= LC_ENEMY_FIRST_HANDLE && handle <= LC_ENEMY_LAST_HANDLE &&
           lc_enemy_types[handle] == LC_MEMBER_SOLDIER;
}

int lc_member_is_soldier(unsigned int source, unsigned int handle)
{
    if (source < LC_PLAYER_SOURCES) return handle >= 100;
    return lc_enemy_is_soldier(handle);
}

int lc_enemy_allocate(unsigned int kind)
{
    unsigned int handle;
    if (kind != LC_MEMBER_ORDINARY && kind != LC_MEMBER_SOLDIER) return 0;
    for (handle = LC_ENEMY_FIRST_HANDLE; handle <= LC_ENEMY_LAST_HANDLE; ++handle) {
        if (lc_enemy_types[handle] == LC_MEMBER_FREE) {
            lc_enemy_types[handle] = (unsigned char)kind;
            return handle;
        }
    }
    return 0;
}

void lc_enemy_release(unsigned int handle)
{
    if (handle < LC_ENEMY_FIRST_HANDLE || handle > LC_ENEMY_LAST_HANDLE) return;
    lc_capacity_clear(lc_enemy_characters[handle], 52);
    lc_enemy_soldier_hp[handle] = 0;
    lc_enemy_soldier_state[handle] = 0;
    lc_enemy_types[handle] = LC_MEMBER_FREE;
}

void lc_enemy_reset(void)
{
    lc_capacity_clear(lc_enemy_characters, LC_ENEMY_HANDLES * 52);
    lc_capacity_clear(lc_enemy_types, LC_ENEMY_HANDLES);
    lc_capacity_clear(lc_enemy_soldier_hp, LC_ENEMY_HANDLES * 2);
    lc_capacity_clear(lc_enemy_soldier_state, LC_ENEMY_HANDLES);
}

void lc_enemy_clear_members(void)
{
    /* Cleanup retains the Soldier template in record0. All allocatable handles,
     * including retail's inconsistently cleared99, lose their old ownership. */
    lc_capacity_clear(lc_enemy_characters[1], (LC_ENEMY_HANDLES - 1) * 52);
    lc_capacity_clear(lc_enemy_types, LC_ENEMY_HANDLES);
    lc_capacity_clear(lc_enemy_soldier_hp, LC_ENEMY_HANDLES * 2);
    lc_capacity_clear(lc_enemy_soldier_state, LC_ENEMY_HANDLES);
}

/* Enemy branch of accepted func_001957D0: same template selectors, formation
 * positions, faction flag, character initializer, supply clear and finalizer.
 * Preflight every member before changing the source or publishing allocations.
 * No failed scan can call the character initializer with index-1.
 */
int lc_enemy_construct(const unsigned char *spec, unsigned int source)
{
    unsigned int id = source & 127U, flag, count = 1, selector, slot, i, free_count;
    unsigned char kinds[5], selectors[5], positions[5], handles[5];
    unsigned char *record;
    if (!spec || id >= LC_MAP_SOURCE_END) return 0;
    if (id < LC_PLAYER_SOURCES) {
        lc_capacity_player_construct(spec, source);
        return 1;
    }
    kinds[0] = LC_MEMBER_ORDINARY;
    selectors[0] = 0;
    positions[0] = (unsigned char)(spec[6] - 1);
    for (selector = 1; selector <= 2; ++selector) {
        unsigned int offset = selector == 1 ? 7 : 16;
        if (!spec[offset]) continue;
        for (slot = 0; slot < 3 && spec[offset + 6 + slot]; ++slot) {
            if (count == 5) return 0;
            kinds[count] = spec[offset] == 1 ? LC_MEMBER_SOLDIER : LC_MEMBER_ORDINARY;
            selectors[count] = (unsigned char)selector;
            positions[count++] = (unsigned char)(spec[offset + 6 + slot] - 1);
        }
    }
    free_count = 0;
    for (i = LC_ENEMY_FIRST_HANDLE; i <= LC_ENEMY_LAST_HANDLE && free_count < count; ++i)
        if (lc_enemy_types[i] == LC_MEMBER_FREE) handles[free_count++] = (unsigned char)i;
    if (free_count != count) return 0;

    flag = !(source & 128U);
    record = lc_capacity_sources[id];
    lc_capacity_active_sources[id] = (unsigned char)id;
    record[0] = (unsigned char)(id + 1);
    record[1] = flag ? 0x81 : 1;
    lc_capacity_clear(record + 2, 5);
    for (i = 0; i < count; ++i) {
        unsigned int handle = handles[i];
        lc_enemy_types[handle] = kinds[i];
        if (kinds[i] == LC_MEMBER_SOLDIER) {
            lc_enemy_soldier_hp[handle] = lc_enemy_soldier_max_hp;
            lc_enemy_soldier_state[handle] = 0;
        } else {
            lc_capacity_character(handle, spec, selectors[i], (unsigned char)flag, (unsigned char)id);
        }
        /* The LS per-follower initializer counts already-published members. */
        record[2 + i] = (unsigned char)handle;
        record[7 + i] = positions[i];
    }
    record[24] = lc_capacity_finalize(record);
    lc_capacity_clear(record + 13, 10);
    return 1;
}
