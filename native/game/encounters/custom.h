/* Native custom encounter contract. Authored profiles compile during export.
 * This is maintained extension code, not a claim of accepted decompiled C. */
#ifndef LC_CUSTOM_ENCOUNTER_H
#define LC_CUSTOM_ENCOUNTER_H
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef struct {
    u8 class_id, level_offset, cell, reserved;
    u16 equipment_a, equipment_b;
} LcCustomMember;
typedef struct { u8 class_id, bonus; } LcLeaderBonus;
typedef struct { u16 item, weight; } LcCustomReward;
typedef struct {
    u8 enabled, member_count, persuasion, chance;
    u8 retreat_hp, runtime_key, terrain_slot, reserved;
    LcCustomMember member[5];
    LcLeaderBonus bonus[3];
    u16 reserved54;
    LcCustomReward reward[3];
    /* Encoded @b@%0@n"text"@a@c plus NUL, maximum 24 authored characters. */
    u8 message[40];
    u8 reserved108[20];
} LcCustomProfile;
typedef struct {
    u32 pending, active, persuaded, talk_record;
    LcCustomProfile profile;
} LcCustomState;
typedef char LcProfileSizeCheck[sizeof(LcCustomProfile) == 128 ? 1 : -1];
typedef char LcStateSizeCheck[sizeof(LcCustomState) == 144 ? 1 : -1];
/* Linker-bound to an explicitly reserved resident allocation and ROM library. */
extern LcCustomState lc_custom_state;
extern u8 lc_custom_index[];
extern u8 lc_staged_scenario;
extern u8 lc_character_records[];
extern u8 lc_source_records[];
/* Match the transient reset at z64 0x45F30: slots 1..98. Slot zero is
 * reserved; 99 is outside that reset. IDs 100+ have another reference meaning. */
#define LC_CUSTOM_CHARACTER_END 99
extern void lc_dma_read(u32 source, void *destination, u32 length);
extern u32 lc_random(void);
extern void lc_character_create(void *record, u32 class_id, u32 level,
                                u32 equipment_a, u32 equipment_b);
extern u8 *lc_actor_at_index(u32 index);
extern s32 lc_persuasion_filter_a(const u8 *actor);
extern s32 lc_persuasion_filter_b(const u8 *actor);
extern s32 lc_persuasion_filter_c(const u8 *actor);
extern s32 lc_persuasion_filter_d(const u8 *actor);
extern s32 lc_persuasion_eligible(const u8 *actor);
extern u32 lc_persuasion_random(void);
#endif
