typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;
typedef float f32;
#define FIELD(p, type, offset) (*(type *)((u8 *)(p) + (offset)))

extern u8 D_8018F481, D_800E7AC3;
extern u8 D_801E7E18[];
extern u8 D_802305CC, D_802305CD;
extern u16 D_802305C0;
extern volatile s32 D_801F0E00;
extern s32 D_801F0E04, D_8018F58C;
extern s32 D_801F0DE0, D_801F0DFC, D_801F3610;
extern u8 D_801F365D, D_801976BA, D_801968BD;
extern u8 *D_801F0CB0[];
extern u8 D_800E7A90[];
extern u8 D_801ED740[], D_801ED760[], D_801ED77E[], D_801F0AA0[];
extern s32 func_0016C454(void);
extern s32 func_0005c110(s32);
extern s32 func_00022e60(void);
extern void func_00023940(void *, ...);
extern void func_0010D484(f32, f32, f32, s32, s32);
extern s32 func_801D935C(void *), func_801D92D0(void *);
extern s32 func_001314D0(void *, u16);

/* KMC keeps the original bound snapshot when this helper's reduced-bound
 * result is unused. The caller advances its two outer counters itself. */
static __inline__ s32 remove_candidate(u8 *candidates, u8 *next, s32 index, s32 last)
{
    if (index < last) {
        {
            u8 *to = (u8 *)((u32)index + (u32)candidates);
            u8 *from = (u8 *)((u32)index + (u32)next);

        copy_loop:
            *to = *from++;
            index++;
            to++;
            if (index < last) {
                goto copy_loop;
            }
        }
    }
    last--;
    return last;
}

void func_0013C060(void)
{
    u8 candidates[30];
    s32 index;
    s32 remaining;
    register s32 last;
    s32 selected;
    s32 weight;
    s32 row;
    s32 value;
    s32 enabled;
    u32 threshold;
    u32 interval;
    u8 *unit;
    u8 *lookup;
    u8 *lookup_table;
    u8 *value_table;
    u8 *initial;
    s32 initial_index;
    u8 *candidate_base;
    u8 *candidate_next;
    register volatile u8 *selector;

    if (D_801E7E18[D_8018F481 * 12] < 3) {
        return;
    }
    if (func_0016C454() && D_8018F481 == 0x3F && D_802305CD) {
        u8 *early_unit;
        s32 early_value, early_parameter, early_index;
        register s32 early_code;

        D_801F0E00 = D_802305CC;
        /* Single-pass scopes preserve retail load, argument-constant and
         * global-store scheduling without adding native instructions. */
        early_index = D_801F0E00;
        do {
            early_value = D_802305C0;
        } while (0);
        early_parameter = D_8018F58C;
        do {
            early_unit = D_801F0CB0[early_index];
            early_value += 0x100;
        } while (0);
        do {
            early_code = 0x2E;
        } while (0);
        D_801F0E04 = early_value;
        func_0010D484(FIELD(early_unit, f32, 8), FIELD(early_unit, f32, 12),
                      FIELD(early_unit, f32, 16), early_parameter, early_code);
        return;
    }
    if (D_801F0DE0 || D_801F0DFC || D_801F3610 ||
        (D_801F365D & 2) || (D_801976BA & 1) || D_801968BD) {
        return;
    }

    /* Keeping the divisor across the query retains the retail DIVU form. */
    interval = 72000;
    enabled = func_0005c110(0x11);
    threshold = 50;
    if (!enabled) {
        threshold = 10;
    }
    if ((u32)func_00022e60() % interval > threshold) {
        return;
    }
    /* The scope placement also retains the reverse pointer's retail
     * allocation priority relative to its integer counter. */
    do {
        initial_index = 29;
    } while (0);
    initial = candidates + 29;
    do {
        *initial = initial_index;
        initial_index--;
        do {
            do {
                do {
                    initial = (u8 *)((u32)initial - 1);
                } while (0);
            } while (0);
        } while (0);
    } while (initial_index >= 0);
    /* These invariant bases rematerialize in T0; selector stays in S8. */
    lookup_table = D_800E7A90;
    value_table = D_801ED77E;
    remaining = 30;
    candidate_base = candidates;
    do {
        do {
            selector = &D_800E7AC3;
        } while (0);
    } while (0);
    candidate_next = candidates + 1;
    do {
        last = 29;
    candidate_loop:
        do {
            index = func_00022e60() % remaining;
        } while (0);
        selected = candidate_base[index];
        if ((u32)selected >= 30) {
            func_00023940(D_801F0AA0);
        }
        unit = D_801F0CB0[selected];
        if ((FIELD(unit, u32, 0) & 0x60011) == 0x40011 &&
            FIELD(unit, s32, 0x78) == 0 && !func_801D935C(unit) &&
            func_801D92D0(unit)) {
            lookup = *(u8 **)(lookup_table + *selector * 4);
            weight = D_801ED740[lookup[FIELD(unit, s32, 0x14)]];
            if (func_001314D0(unit, 0xF2)) {
                weight *= 2;
            }
            if (func_00022e60() % 100 < weight) {
                s32 random;
                s32 kind;
                u8 *values;
                s32 offset, biased;

                /* Reusing offset for signed parity and row indexing keeps
                 * KMC from scheduling the row early. A separate biased value
                 * retains the dividend-first parity ADD. */
                do {
                    row = D_801E7E18[D_8018F481 * 12] - 3;
                    random = func_00022e60();
                    offset = FIELD(unit, s32, 0x14);
                    offset = lookup[offset];
                    kind = D_801ED760[offset];
                    offset = (u32)random >> 31;
                    biased = random + offset;
                    offset = biased;
                    offset >>= 1;
                    offset *= 2;
                    random -= offset;
                    offset = row * 4;
                    offset += row;
                    offset *= 4;
                    values = (u8 *)((u32)offset + (u32)value_table);
                    values = (u8 *)((u32)values + random);
                    kind *= 2;
                    values = (u8 *)((u32)values + kind);
                    value = *values;
                } while (0);

                if (value != 0) {
                    D_801F0E04 = value + 0x100;
                    D_801F0E00 = selected;
                    func_0010D484(FIELD(unit, f32, 8), FIELD(unit, f32, 12),
                                  FIELD(unit, f32, 16), D_8018F58C, 0x2E);
                    return;
                }
            }
        }
        if (index != 29) {
            remove_candidate(candidate_base, candidate_next, index, last);
        }
        remaining--;
        last--;
        if (remaining != 0) {
            goto candidate_loop;
        }
    } while (0);
}
