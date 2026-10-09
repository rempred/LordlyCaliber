typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef struct RuntimeUnit RuntimeUnit;

#define UNIT_FIELD(type, offset) (*(type *)((u8 *)unit + (offset)))
#define OTHER_FIELD(type, offset) (*(type *)((u8 *)other + (offset)))

extern u8 D_801F0E1A[][10];
extern u8 D_801F0E1B[][10];
extern u8 D_801F0E1C[][10];
extern u8 D_801F0E1D[][10];
extern u8 D_801F0E1E[][10];
extern u8 D_801F0E1F[][10];
extern RuntimeUnit *D_801F0CB0[];
extern u8 D_801971F2[][25];
extern u16 D_80195578[][26];
extern u16 D_80195576[][26];
extern u8 D_80195593[][52];
extern u8 D_80195592[][52];
extern u16 D_80196A2C;
extern u16 D_801F0EBA;
extern f32 D_801F0D98;
extern f32 D_801F0D9C;
extern f32 D_801F0DA0;
extern f32 D_801F0DA4;
/* Retained edge-only overlay interface, with the retail call's explicit a0 input. */
extern s32 func_801DD2B0(RuntimeUnit *unit);

s32 func_00123458(s32 row, RuntimeUnit *unit)
{
    s32 kind;
    /* Both coordinate producers feed the same integer cast/comparison tail. */
    s32 cell_10, cell_08;
    RuntimeUnit *other;

    row--;
    kind = D_801F0E1A[row][0];
    if (kind == 27) {
        if (!(UNIT_FIELD(u32, 0x00) & 2)) {
            if (UNIT_FIELD(s32, 0x74) == D_801F0E1F[row][0] - 1) return 1;
        }
        return 0;
    } else if (kind == 25) {
        s32 valid = 1;
        u8 *member = D_801971F2[UNIT_FIELD(u8, 0x04)];
        u8 *end = member + 5;
        do {
            s32 id = *member;
            if (id != 0) {
                u32 present;
                if (id >= 100) goto next_member;
                present = D_80195578[id][0];
                if (present != 0 && !(D_80195593[id][0] & 4)) {
                    valid &= -(present >= D_80195576[id][0]);
                    valid &= -(D_80195592[id][0] == 0);
                }
            }
next_member:
            member++;
        } while ((s32)member < (s32)end);
        return valid != 0;
    } else if (kind == 26) {
        u32 flags;
        if (func_801DD2B0(unit) != 0) {
            flags = UNIT_FIELD(u32, 0x00);
            if (flags & 0x01000000) {
                UNIT_FIELD(u32, 0x00) = flags & ~0x01000000;
                return 1;
            }
        }
        return 0;
    } else if (kind == 24) {
        u32 state = UNIT_FIELD(u8, 0x20);
        if (state == UNIT_FIELD(s32, 0x24)) return 1;
        return state == 0;
    } else if (kind == 16) {
        f32 scaled_10, scaled_08;
        u32 lower_10, upper_08;
        f32 span_08;
        span_08 = D_801F0DA0 - D_801F0D98;
        scaled_10 = UNIT_FIELD(f32, 0x10);
        scaled_10 -= D_801F0D9C;
        scaled_10 *= 256.0f;
        scaled_10 /= D_801F0DA4 - D_801F0D9C;
        scaled_08 = ((UNIT_FIELD(f32, 0x08) - D_801F0D98) * 256.0f) / span_08;
        /* These are the E1C/E1D bytes through the existing ten-byte row view.
           The distinct view keeps this producer separate until the shared tail. */
        lower_10 = D_801F0E1A[row][2];
        upper_08 = D_801F0E1A[row][3];
        {
            u32 lower_08, upper_10;
            cell_10 = (s32)scaled_10;
            cell_08 = (s32)scaled_08;
            lower_08 = D_801F0E1B[row][0];
            upper_10 = D_801F0E1E[row][0];
            if ((cell_08 >= (s32)lower_08) & ((s32)upper_08 >= cell_08)) {
                if ((cell_10 >= (s32)lower_10) & ((s32)upper_10 >= cell_10)) {
                    return 1;
                }
            }
            return 0;
        }
    } else if (kind == 23) {
        f32 scaled_10, scaled_08;
        u32 lower_10, upper_08;
        f32 span_08;
        other = D_801F0CB0[D_801F0E1F[row][0]];
        span_08 = D_801F0DA0 - D_801F0D98;
        scaled_10 = ((OTHER_FIELD(f32, 0x10) - D_801F0D9C) * 256.0f) /
                    (D_801F0DA4 - D_801F0D9C);
        scaled_08 = ((OTHER_FIELD(f32, 0x08) - D_801F0D98) * 256.0f) / span_08;
        lower_10 = D_801F0E1C[row][0];
        upper_08 = D_801F0E1D[row][0];
        {
            u32 lower_08, upper_10;
            cell_10 = (s32)scaled_10;
            cell_08 = (s32)scaled_08;
            lower_08 = D_801F0E1B[row][0];
            upper_10 = D_801F0E1E[row][0];
            if ((cell_08 >= (s32)lower_08) & ((s32)upper_08 >= cell_08)) {
                if ((cell_10 >= (s32)lower_10) & ((s32)upper_10 >= cell_10)) {
                    return 1;
                }
            }
            return 0;
        }
    } else if (kind == 21) {
        u32 flags;
        flags = UNIT_FIELD(u32, 0x00);
        if ((s32)flags < 0) {
            UNIT_FIELD(u32, 0x00) = flags & 0x7FFFFFFF;
            return 1;
        }
        return 0;
    } else if (kind == 22) {
        u32 flags;
        flags = UNIT_FIELD(u32, 0x00);
        if (!(flags & 0x00040000) && D_80196A2C == D_801F0E1F[row][0]) {
            UNIT_FIELD(u32, 0x00) = flags | 0x00040000;
            return 1;
        }
        return 0;
    } else {
        u16 *event_bits = &D_801F0EBA;
        s32 bits = *event_bits;
        if ((bits >> row) & 1) {
            if (kind == 9) *event_bits = bits & ~(1 << row);
            return 1;
        }
        return 0;
    }
}
