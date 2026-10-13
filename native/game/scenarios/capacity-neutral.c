#include "enemy-capacity.h"

#ifdef LC_CAPACITY_CUSTOM
#include "custom.h"
#endif

extern unsigned short lc_enemy_soldier_max_hp;
extern void lc_capacity_clear(void *data, int size);
extern void lc_capacity_create_record(void *record, unsigned int class_id,
    unsigned int level, unsigned int equipment_a, unsigned int equipment_b);
extern int lc_capacity_original_persuasion(void);
extern unsigned char *lc_capacity_original_talk_target(void);

/* These are fixed game globals, not the relocated arrays. */
#define CURRENT_ENEMY (*(volatile unsigned char *)0x801976E8U)
#define ENCOUNTER_LEVEL (*(volatile unsigned char *)0x801936ACU)
#define SPECIAL_ENCOUNTER (*(volatile unsigned char *)0x801936E0U)
#define ENCOUNTER_KEY (*(volatile unsigned char *)0x801936A7U)
#define ENCOUNTER_ACTIVE (*(volatile unsigned char *)0x80193671U)

static unsigned int clear_neutral(void)
{
    unsigned int slot, handle, class_id = 0;
    unsigned char *source = lc_capacity_sources[LC_NEUTRAL_SOURCE];
    for (slot = 0; slot < 5; ++slot) {
        handle = source[slot + 2];
        if (!handle || handle > LC_ENEMY_LAST_HANDLE) continue;
        if (lc_enemy_types[handle] == LC_MEMBER_ORDINARY)
            class_id = lc_enemy_characters[handle][18];
        else if (lc_enemy_types[handle] == LC_MEMBER_SOLDIER) class_id = 1;
        lc_enemy_release(handle);
    }
    lc_capacity_clear(source, 25);
    return class_id;
}

/* The old backwards search could choose player source29 when map sources were
 * full. Source60 has independent storage, is saved, and is never authorable.
 * A valid map uses at most150 of254 handles; even a five-member encounter fits.
 */
void lc_capacity_neutral_construct(void)
{
    unsigned int class_id = CURRENT_ENEMY, level = ENCOUNTER_LEVEL, handle;
    unsigned int special = SPECIAL_ENCOUNTER && ENCOUNTER_KEY == 63;
    unsigned char *source = lc_capacity_sources[LC_NEUTRAL_SOURCE];
    clear_neutral();
    CURRENT_ENEMY = LC_NEUTRAL_SOURCE;
    source[1] = 0x81;
    if (special) level += 15;
#ifdef LC_CAPACITY_CUSTOM
    if (lc_custom_materialize(source, level)) return;
#endif
    handle = lc_enemy_allocate(class_id == 1 ? LC_MEMBER_SOLDIER : LC_MEMBER_ORDINARY);
    /* Never index record0 after a corrupt/exhausted pool. Valid full armies have
     * ample handles; this guard does not claim to cancel a staged encounter. */
    if (!handle) return;
    if (class_id == 1) {
        lc_enemy_soldier_hp[handle] = lc_enemy_soldier_max_hp;
        lc_enemy_soldier_state[handle] = 1;
    } else {
        lc_capacity_create_record(lc_enemy_characters[handle], class_id, level, 0, 0);
    }
    source[3] = (unsigned char)handle;
    source[8] = special ? 7 : 4;
}

void lc_capacity_neutral_cleanup(void)
{
    unsigned int class_id = clear_neutral();
    CURRENT_ENEMY = (unsigned char)class_id;
    ENCOUNTER_ACTIVE = 0;
}

/* The ordinary deletion helper is also called by scripts which do not detach
 * the member's source reference. Retain its tag until that reference is gone;
 * immediate reuse would make the old squad point to a different character.
 */
void lc_capacity_release_detached(unsigned int pool, unsigned int handle)
{
    unsigned int source, slot;
    if (pool < LC_PLAYER_SOURCES || !handle || handle > LC_ENEMY_LAST_HANDLE) return;
    for (source = LC_PLAYER_SOURCES; source < LC_TOTAL_SOURCES; ++source)
        for (slot = 0; slot < 5; ++slot)
            if (lc_capacity_sources[source][slot + 2] == handle) return;
    lc_enemy_release(handle);
}

void lc_capacity_delete_enemy(unsigned int handle)
{
    if (!handle || handle > LC_ENEMY_LAST_HANDLE) return;
    lc_enemy_characters[handle][17] = 0;
    lc_enemy_characters[handle][18] = 0;
    lc_enemy_characters[handle][51] = 0;
    lc_capacity_release_detached(LC_PLAYER_SOURCES, handle);
}

unsigned int lc_capacity_find_enemy(unsigned int class_id)
{
    unsigned int handle;
    for (handle = 1; handle <= LC_ENEMY_LAST_HANDLE; ++handle)
        if (lc_enemy_types[handle] == LC_MEMBER_ORDINARY &&
            lc_enemy_characters[handle][17] == class_id) return handle;
    return 255;
}

/* Keep the game's failed-persuasion presentation when no player slot exists.
 * The original recruitment routine otherwise falls back to overwriting slot0.
 * This runs before it reports success, not after a destructive copy begins.
 */
int lc_capacity_persuasion(void)
{
    unsigned int id;
    unsigned char *target;
    int result = lc_capacity_original_persuasion();
    if (result != 0) return result;
    target = lc_capacity_original_talk_target();
#ifdef LC_CAPACITY_CUSTOM
    if (lc_custom_state.active && lc_custom_state.talk_record)
        target = (unsigned char *)lc_custom_state.talk_record;
#endif
    if (!target || !target[246] || target[246] > LC_ENEMY_LAST_HANDLE ||
        lc_enemy_types[target[246]] != LC_MEMBER_ORDINARY) {
#ifdef LC_CAPACITY_CUSTOM
        lc_custom_state.persuaded = 0;
#endif
        return 2;
    }
    for (id = 1; id < 100; ++id)
        if (!*((volatile unsigned char *)0x80193BC0U + id * 56 + 17)) return 0;
#ifdef LC_CAPACITY_CUSTOM
    lc_custom_state.persuaded = 0;
#endif
    return 2;
}

/* Scripted copies retain their enemy originals. Preflight every actual occupied
 * member slot and provide a player ID for that same slot; holes never index an
 * uninitialized halfword. Soldiers require an explicit conversion, so reject
 * them here instead of treating unrelated52-byte storage as a character.
 */
int lc_capacity_transfer_slots(unsigned int source, unsigned short *slots)
{
    unsigned int slot, handle, player = 1;
    unsigned short selected[5];
    if (source < LC_PLAYER_SOURCES || source >= LC_MAP_SOURCE_END) return 0;
    for (slot = 0; slot < 5; ++slot) {
        selected[slot] = 0;
        handle = lc_capacity_sources[source][slot + 2];
        if (!handle) continue;
        if (handle > LC_ENEMY_LAST_HANDLE || lc_enemy_types[handle] != LC_MEMBER_ORDINARY) return 0;
        while (player < 100 && *((volatile unsigned char *)0x80193BC0U + player * 56 + 17)) ++player;
        if (player == 100) return 0;
        selected[slot] = (unsigned short)player++;
    }
    for (slot = 0; slot < 5; ++slot) slots[slot] = selected[slot];
    return 1;
}
