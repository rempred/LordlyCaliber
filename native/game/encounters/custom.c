#include "custom.h"

/* A direct ROM index has 256 keys by ten terrain slots, one source offset each.
 * Zero keeps the retail pair. The loaded profile is shared by the encounter's
 * materializer, card, battle controls and final reward; no library is resident. */
u32 lc_custom_select(u32 retail_class, u32 pool, u32 pair_address)
{
    u32 start, slot, source, key;
    LcCustomState *state = &lc_custom_state;
    state->pending = 0;
    state->active = 0;
    state->persuaded = 0;
    state->talk_record = 0;
    if (pool < 1 || pool > 45) return retail_class;
    start = 0x801ED794U + (pool - 1) * 20;
    if (pair_address < start || pair_address >= start + 20) return retail_class;
    slot = (pair_address - start) >> 1;
    key = lc_staged_scenario;
    lc_dma_read((u32)lc_custom_index + (key * 10 + slot) * 4, &source, 4);
    if (source == 0) return retail_class;
    lc_dma_read(source, &state->profile, sizeof(LcCustomProfile));
    if (state->profile.enabled != 1 || state->profile.member_count < 2 ||
        state->profile.member_count > 5 || state->profile.runtime_key != key ||
        state->profile.terrain_slot != slot || state->profile.member[0].class_id < 2)
        return retail_class;
    state->pending = 1;
    state->active = 1;
    return state->profile.member[0].class_id;
}

/* Return1 after constructing the complete squad;0 requests the original path.
 * Preflight every slot before any character constructor runs. Game record0 is
 * reserved; records1..119 have stride52 and occupancy byte17. */
u32 lc_custom_materialize(u8 *source, u32 base_level)
{
    LcCustomState *state = &lc_custom_state;
    u32 available, id, member, count, free_id[5];
    u8 *record;
    const LcCustomMember *spec;
    if (!state->pending || !state->active) return 0;
    state->pending = 0;
    count = state->profile.member_count;
    if (count < 2 || count > 5) {
        state->active = 0;
        return 0;
    }
    available = 0;
    for (id = 1; id < 120 && available < count; ++id)
        if (!lc_character_records[id * 52 + 17]) free_id[available++] = id;
    if (available < count) {
        state->active = 0;
        return 0;
    }
    for (member = 0; member < count; ++member) {
        id = free_id[member];
        record = lc_character_records + id * 52;
        spec = &state->profile.member[member];
        lc_character_create(record, spec->class_id,
            (base_level + spec->level_offset) & 255, spec->equipment_a, spec->equipment_b);
        source[member + 2] = id;
        source[member + 7] = spec->cell;
    }
    return 1;
}

/* -1 preserves the class reward path. Persuasion consumes the active encounter
 * without a victory reward. Every active result clears pending/target state. */
s32 lc_custom_reward(void)
{
    LcCustomState *state = &lc_custom_state;
    u32 total, roll, i;
    s32 result;
    if (!state->active) return -1;
    result = 0;
    if (!state->persuaded) {
        total = 0;
        for (i = 0; i < 3; ++i) total += state->profile.reward[i].weight;
        if (total) {
            roll = lc_random() % total;
            for (i = 0; i < 3; ++i) {
                if (roll < state->profile.reward[i].weight) {
                    result = state->profile.reward[i].item;
                    break;
                }
                roll -= state->profile.reward[i].weight;
            }
        }
    }
    state->pending = 0;
    state->active = 0;
    state->persuaded = 0;
    state->talk_record = 0;
    return result;
}

/* The caller retains the original message namespace checks. */
const u8 *lc_custom_message(void)
{
    return lc_custom_state.active ? lc_custom_state.profile.message : (const u8 *)0;
}
