#include "game/scenario_item_views.h"

/* Partial pool view: maximum is also D_80190EBC[99]; target IDs100..255 follow it. */
extern u16 D_80190EBC[];
extern u8 D_80191010[];
extern u16 D_801953F2;
extern u16 D_8019532C[];
extern s32 rand(void);
struct CharacterRecord;
extern void func_00044aa4(struct CharacterRecord *);

/* Parameterized remainder retains the signed division used by this compiler. */
static inline s32 random_bounded(s32 divisor)
{
    return rand() % divisor;
}
static inline s32 above(s32 value, s32 maximum) { return value > maximum; }
/* Phase zero generates the value that callers retain for later application. */
s32 func_00041098(u8 apply, s32 value, u16 item, u8 target)
{
    ScenarioCharacter *character = &D_80193BC0[target];
    s32 result = 0;
    u16 amount;
    s32 current;
    u16 *stat;
    u8 *byte_stat;
    switch (item) {
    case 1:
    case 2:
        amount = 100; if (item != 1) amount = 300;
        result = amount;
        goto heal;
    case 3:
        amount = 150;
        result = 150;
heal:
        if (apply) {
            if (target >= 100) {
                u16 step = D_80190EBC[99] / 3U;
                /* Widen after narrowing: keep the cap unit distinct from the countdown step. */
                s32 segment_size = (u16)(D_80190F82.pool.maximum / 3U);
                s16 remaining = D_80190F82.pool.values[target - 100];
                s16 count = 0;
                s32 segments;
                u16 limit;
                while (remaining > 0) { remaining -= step; count++; }
                if (count >= 4) count = 3;
                segments = (u8)count;
                if ((u32)segments >= 3) limit = D_80190F82.pool.maximum;
                else limit = segment_size * segments;
                if (limit < D_80190EBC[target] + amount) D_80190EBC[target] = limit;
                else D_80190EBC[target] = D_80190EBC[target] + amount;
            } else {
                u16 hp = character->field18;
                if (hp != 0) {
                    u16 maximum = character->field16;
                    u16 healed;
                    if (maximum < hp + value) healed = maximum;
                    else healed = hp + value;
                    character->field18 = healed;
                }
            }
        }
        break;
    case 4:
    case 5:
        amount = 20; if (item != 4) amount = 50;
        result = amount;
        if (apply) {
            if (target < 100) {
                u8 fatigue;
                if (character->field32 - value < 0) fatigue = 0;
                else fatigue = character->field32 - value;
                character->field32 = fatigue;
            }
        }
        break;
    case 6:
        if (apply) {
            if (target >= 100) D_80191010[target] &= 0xFB;
            else character->field33 &= 0xFB;
        }
        break;
    case 7:
        result = character->field16;
        if (apply) {
            u16 hp = character->field18;
            if (result >= hp + value) hp = hp + value;
            else hp = character->field16;
            character->field18 = hp;
            character->field33 &= 0xFB;
        }
        break;
    case 9:
        if (!apply) { rand(); result = 1; }
        else { func_00044aa4((struct CharacterRecord *)character); result = 0; }
        break;
    /* Separate cases preserve the compiler-shared generation and clamp tails. */
    case 10:
        current = character->field16;
        stat = &character->field16;
        if (!apply) result = random_bounded(6) + (5);
        else {
            current += (s16)value;
            if (above(current, 999)) current = 999;
            else if (current < 0) current = 0;
            if (apply) *stat = current;
            result = 0;
        }
        break;    case 11:
        current = character->fields1C[0];
        stat = &character->fields1C[0];
        if (!apply) result = random_bounded(3) + (2);
        else {
            current += (s16)value;
            if (above(current, 999)) current = 999;
            else if (current < 0) current = 0;
            if (apply) *stat = current;
            result = 0;
        }
        break;    case 12:
        current = character->fields1C[1];
        stat = &character->fields1C[1];
        if (!apply) result = random_bounded(3) + (2);
        else {
            current += (s16)value;
            if (above(current, 999)) current = 999;
            else if (current < 0) current = 0;
            if (apply) *stat = current;
            result = 0;
        }
        break;    case 13:
        current = character->fields1C[2];
        stat = &character->fields1C[2];
        if (!apply) result = random_bounded(3) + (2);
        else {
            current += (s16)value;
            if (above(current, 999)) current = 999;
            else if (current < 0) current = 0;
            if (apply) *stat = current;
            result = 0;
        }
        break;    case 14:
        current = character->fields1C[3];
        stat = &character->fields1C[3];
        if (!apply) result = random_bounded(3) + (2);
        else {
            current += (s16)value;
            if (above(current, 999)) current = 999;
            else if (current < 0) current = 0;
            if (apply) *stat = current;
            result = 0;
        }
        break;    case 15:
        current = character->fields1C[4];
        stat = &character->fields1C[4];
        if (!apply) result = random_bounded(3) + (2);
        else {
            current += (s16)value;
            if (above(current, 999)) current = 999;
            else if (current < 0) current = 0;
            if (apply) *stat = current;
            result = 0;
        }
        break;    case 16:
        current = character->fields1C[5];
        stat = &character->fields1C[5];
        if (!apply) result = random_bounded(3) + (2);
        else {
            current += (s16)value;
            if (above(current, 999)) current = 999;
            else if (current < 0) current = 0;
            if (apply) *stat = current;
            result = 0;
        }
        break;    case 17:
        current = character->field1B;
        byte_stat = &character->field1B;
        if (!apply) result = random_bounded(3) + (1);
        else {
            current += (s16)value;
            if (above(current, 100)) current = 100;
            else if (current < 0) current = 0;
            if (apply) *byte_stat = current;
            result = 0;
        }
        break;    case 18:
        current = character->field1B;
        byte_stat = &character->field1B;
        if (!apply) result = random_bounded(3) + (-3);
        else {
            current += (s16)value;
            if (above(current, 100)) current = 100;
            else if (current < 0) current = 0;
            if (apply) *byte_stat = current;
            result = 0;
        }
        break;    case 19:
        current = character->field28;
        byte_stat = &character->field28;
        if (!apply) result = random_bounded(3) + (-1);
        else {
            current += (s16)value;
            if (above(current, 100)) current = 100;
            else if (current < 0) current = 0;
            if (apply) *byte_stat = current;
            result = 0;
        }
        break;
    }
    return result;
}












