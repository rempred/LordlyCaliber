/* Maintained adaptation of accepted func_00041098.c. The original C remains in
 * game/accepted/library. Preserve query/apply semantics and special-pool caps;
 * consolidate the six identical stat cases and read authored parameters as data.
 * Parameter order is the catalog's five magnitudes followed by five min/width
 * pairs. This is modified-game C, not a claim of matching retail instructions. */
#include "game/scenario_item_views.h"
extern const u16 lc_effect_parameters[15];
extern u16 D_80190EBC[];
extern u8 D_80191010[];
extern s32 rand(void);
struct CharacterRecord;
extern void func_00044aa4(struct CharacterRecord *);

s32 func_00041098(u8 apply, s32 value, u16 item, u8 target)
{
    ScenarioCharacter *character = &D_80193BC0[target];
    u16 amount;
    s32 current, maximum, parameter;
    u16 *stat;
    u8 *byte_stat;
    if (item >= 1 && item <= 3) {
        amount = lc_effect_parameters[item - 1];
        if (apply) {
            if (target >= 100) {
                u16 step = D_80190EBC[99] / 3U;
                s32 segment_size = (u16)(D_80190F82.pool.maximum / 3U);
                s16 remaining = D_80190F82.pool.values[target - 100];
                s16 count = 0;
                u16 limit;
                while (remaining > 0) { remaining -= step; count++; }
                if (count >= 4) count = 3;
                if ((u32)(u8)count >= 3) limit = D_80190F82.pool.maximum;
                else limit = segment_size * (u8)count;
                if (limit < D_80190EBC[target] + amount) D_80190EBC[target] = limit;
                else D_80190EBC[target] += amount;
            } else if (character->field18 != 0) {
                if (character->field16 < character->field18 + value)
                    character->field18 = character->field16;
                else character->field18 += value;
            }
        }
        return amount;
    }
    if (item == 4 || item == 5) {
        amount = lc_effect_parameters[item - 1];
        if (apply && target < 100) {
            if (character->field32 - value < 0) character->field32 = 0;
            else character->field32 -= value;
        }
        return amount;
    }
    if (item == 6) {
        if (apply) {
            if (target >= 100) D_80191010[target] &= 0xFB;
            else character->field33 &= 0xFB;
        }
        return 0;
    }
    if (item == 7) {
        maximum = character->field16;
        if (apply) {
            if (maximum >= character->field18 + value) character->field18 += value;
            else character->field18 = character->field16;
            character->field33 &= 0xFB;
        }
        return maximum;
    }
    if (item == 9) {
        if (!apply) { rand(); return 1; }
        func_00044aa4((struct CharacterRecord *)character);
        return 0;
    }
    if (item < 10 || item > 19) return 0;
    if (item == 10) { parameter = 5; stat = &character->field16; }
    else if (item <= 16) { parameter = 7; stat = &character->fields1C[item - 11]; }
    else parameter = 9 + (item - 17) * 2;
    if (!apply)
        return rand() % (s32)lc_effect_parameters[parameter + 1]
            + (s16)lc_effect_parameters[parameter];
    if (item <= 16) { current = *stat; maximum = 999; }
    else {
        byte_stat = item == 19 ? &character->field28 : &character->field1B;
        current = *byte_stat; maximum = 100;
    }
    current += (s16)value;
    if (current > maximum) current = maximum;
    else if (current < 0) current = 0;
    if (item <= 16) *stat = current;
    else *byte_stat = current;
    return 0;
}
