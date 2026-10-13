#ifndef LORDLY_ENEMY_CAPACITY_H
#define LORDLY_ENEMY_CAPACITY_H

#define LC_PLAYER_SOURCES 30
#define LC_PLAYER_DEPLOYED 20
#define LC_ENEMY_SOURCES 30
#define LC_MAP_SOURCE_END 60
#define LC_NEUTRAL_SOURCE 60
#define LC_ENEMY_SAVE_SOURCES 31
#define LC_TOTAL_SOURCES 61
#define LC_ENEMY_HANDLES 256
#define LC_ENEMY_FIRST_HANDLE 1
#define LC_ENEMY_LAST_HANDLE 254
#define LC_MEMBER_FREE 0
#define LC_MEMBER_ORDINARY 1
#define LC_MEMBER_SOLDIER 2

/* Source/member IDs stay byte-sized. The player keeps its retail namespace.
 * Enemy ordinary characters and Soldier groups share tagged, stable handles;
 * handle0 is the Soldier template, and255 remains a sentinel.
 */
extern unsigned char lc_enemy_types[LC_ENEMY_HANDLES];
extern unsigned char lc_enemy_characters[LC_ENEMY_HANDLES][52];
extern unsigned short lc_enemy_soldier_hp[LC_ENEMY_HANDLES];
extern unsigned char lc_enemy_soldier_state[LC_ENEMY_HANDLES];
extern unsigned char lc_capacity_sources[LC_TOTAL_SOURCES][25];
extern unsigned char lc_capacity_active_sources[LC_TOTAL_SOURCES];

int lc_enemy_is_soldier(unsigned int handle);
int lc_member_is_soldier(unsigned int source, unsigned int handle);
int lc_enemy_allocate(unsigned int kind);
void lc_enemy_release(unsigned int handle);
void lc_enemy_reset(void);
void lc_enemy_clear_members(void);
int lc_enemy_construct(const unsigned char *spec, unsigned int source);

#endif
