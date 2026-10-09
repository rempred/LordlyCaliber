typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef double f64;
#define WAKE_FIELD(base,type,offset) (*(type)((s8 *)(base)+(offset)))
/* Four contiguous float operands form a typed view of the existing data.
   The member accesses reproduce this compiler's per-point alias/reload behavior;
   this does not establish the original source declaration. */
typedef struct WakeBounds {
    f32 field_00;
    f32 field_04;
    f32 field_08;
    f32 field_0C;
} WakeBounds;
extern WakeBounds D_801F0D98;
typedef struct RuntimeUnit RuntimeUnit;
typedef struct { u8 bytes[18]; } WakeRecord18;
extern u8 D_800E7AB9;
extern f64 D_801EE610;
extern f64 D_801EE618;
extern f64 D_801EE620;
extern f64 D_801EE628;
extern f64 D_801EE630;
extern u8 D_801971F1[];

f32 func_000F315C(u8, f32, f32);                    /* extern */
f32 func_000F3428(u8, f32, f32);                    /* extern */
void func_001072B8(void *);                      /* extern */
void func_0012EA80(s32, f32 *);                  /* extern */


/* By-value publishers retain each point across the cleanup call. Count-three
   publishers carry the middle components as raw words, as in the retail copy. */
static inline s32 wake_grid_cell(f32 x, f32 z)
{
    f32 base_z;
    f32 base_x;
    /* Keep the member reads with each point conversion. The typed view retains
       the observed bound reloads across the preceding point stores. */
    base_z = D_801F0D98.field_04;
    z -= base_z;
    z *= 64.0f;
    z /= D_801F0D98.field_0C - base_z;
    base_x = D_801F0D98.field_00;
    x -= base_x;
    x *= 64.0f;
    x /= D_801F0D98.field_08 - base_x;
    return ((s32)z << 6) + (s32)x;
}

static inline void wake_publish1_mode0(void *unit, f32 x0, f32 y0, f32 z0, s32 tag)
{
    u32 flags;
    func_001072B8(unit);
    flags = WAKE_FIELD(unit, u32 *, 0);
    WAKE_FIELD(unit, s32 *, 0x84) = -1;
    WAKE_FIELD(unit, s32 *, 0x80) = -1;
    WAKE_FIELD(unit, u8 *, 0x91) = 0;
    WAKE_FIELD(unit, s32 *, 0x88) = 0;
    WAKE_FIELD(unit, u8 *, 0x92) = 0;
    WAKE_FIELD(unit, u32 *, 0) = flags & ~2;
    WAKE_FIELD(unit, f32 *, 0x28) = x0;
    WAKE_FIELD(unit, f32 *, 0x2c) = y0;
    WAKE_FIELD(unit, f32 *, 0x30) = z0;
    WAKE_FIELD(unit, s32 *, 0x58) = wake_grid_cell(x0, z0);
    WAKE_FIELD(unit, u8 *, 0x20) = 1;
    WAKE_FIELD(unit, s32 *, 0x24) = tag;
    WAKE_FIELD(unit, u32 *, 0) &= ~4;
    WAKE_FIELD(unit, u32 *, 0) &= ~0x200;
    WAKE_FIELD(unit, u32 *, 0) |= 2;
    WAKE_FIELD(unit, u32 *, 0) &= ~0x00800000;
}

static inline void wake_publish1_mode1(void *unit, f32 x0, f32 y0, f32 z0, s32 tag)
{
    u32 flags;
    func_001072B8(unit);
    flags = WAKE_FIELD(unit, u32 *, 0);
    WAKE_FIELD(unit, u8 *, 0x91) = 0;
    WAKE_FIELD(unit, s32 *, 0x84) = -1;
    WAKE_FIELD(unit, s32 *, 0x80) = -1;
    WAKE_FIELD(unit, s32 *, 0x88) = 0;
    WAKE_FIELD(unit, u8 *, 0x92) = 0;
    WAKE_FIELD(unit, u32 *, 0) = flags & ~2;
    WAKE_FIELD(unit, f32 *, 0x28) = x0;
    WAKE_FIELD(unit, f32 *, 0x2c) = y0;
    WAKE_FIELD(unit, f32 *, 0x30) = z0;
    WAKE_FIELD(unit, s32 *, 0x58) = wake_grid_cell(x0, z0);
    WAKE_FIELD(unit, u8 *, 0x20) = 1;
    WAKE_FIELD(unit, u8 *, 0x91) = 1;
    WAKE_FIELD(unit, s32 *, 0x24) = tag;
    WAKE_FIELD(unit, s32 *, 0x84) = -1;
    WAKE_FIELD(unit, u32 *, 0) &= ~4;
    WAKE_FIELD(unit, u32 *, 0) &= ~0x200;
    WAKE_FIELD(unit, u32 *, 0) |= 2;
    WAKE_FIELD(unit, u32 *, 0) &= ~0x00800000;
}

static inline void wake_publish1_mode2(void *unit, f32 x0, f32 y0, f32 z0, s32 tag)
{
    u32 flags;
    func_001072B8(unit);
    flags = WAKE_FIELD(unit, u32 *, 0);
    WAKE_FIELD(unit, u8 *, 0x91) = 0;
    WAKE_FIELD(unit, s32 *, 0x84) = -1;
    WAKE_FIELD(unit, s32 *, 0x80) = -1;
    WAKE_FIELD(unit, s32 *, 0x88) = 0;
    WAKE_FIELD(unit, u8 *, 0x92) = 0;
    WAKE_FIELD(unit, u32 *, 0) = flags & ~2;
    WAKE_FIELD(unit, f32 *, 0x28) = x0;
    WAKE_FIELD(unit, f32 *, 0x2c) = y0;
    WAKE_FIELD(unit, f32 *, 0x30) = z0;
    WAKE_FIELD(unit, s32 *, 0x58) = wake_grid_cell(x0, z0);
    WAKE_FIELD(unit, u8 *, 0x20) = 1;
    WAKE_FIELD(unit, u8 *, 0x91) = 2;
    WAKE_FIELD(unit, s32 *, 0x88) = 90;
    WAKE_FIELD(unit, s32 *, 0x24) = tag;
    WAKE_FIELD(unit, s32 *, 0x84) = -1;
    WAKE_FIELD(unit, u32 *, 0) &= ~4;
    WAKE_FIELD(unit, u32 *, 0) &= ~0x200;
    WAKE_FIELD(unit, u32 *, 0) |= 2;
    WAKE_FIELD(unit, u32 *, 0) &= ~0x00800000;
}

static inline void wake_publish2_mode0(void *unit, f32 x0, f32 y0, f32 z0, f32 x1, f32 y1, f32 z1, s32 tag)
{
    u32 flags;
    func_001072B8(unit);
    flags = WAKE_FIELD(unit, u32 *, 0);
    WAKE_FIELD(unit, s32 *, 0x84) = -1;
    WAKE_FIELD(unit, s32 *, 0x80) = -1;
    WAKE_FIELD(unit, u8 *, 0x91) = 0;
    WAKE_FIELD(unit, s32 *, 0x88) = 0;
    WAKE_FIELD(unit, u8 *, 0x92) = 0;
    WAKE_FIELD(unit, u32 *, 0) = flags & ~2;
    WAKE_FIELD(unit, f32 *, 0x28) = x0;
    WAKE_FIELD(unit, f32 *, 0x2c) = y0;
    WAKE_FIELD(unit, f32 *, 0x30) = z0;
    WAKE_FIELD(unit, s32 *, 0x58) = wake_grid_cell(x0, z0);
    WAKE_FIELD(unit, f32 *, 0x34) = x1;
    WAKE_FIELD(unit, f32 *, 0x38) = y1;
    WAKE_FIELD(unit, f32 *, 0x3c) = z1;
    WAKE_FIELD(unit, s32 *, 0x5c) = wake_grid_cell(x1, z1);
    WAKE_FIELD(unit, u8 *, 0x20) = 2;
    WAKE_FIELD(unit, s32 *, 0x24) = tag;
    WAKE_FIELD(unit, u32 *, 0) &= ~4;
    WAKE_FIELD(unit, u32 *, 0) &= ~0x200;
    WAKE_FIELD(unit, u32 *, 0) |= 2;
    WAKE_FIELD(unit, u32 *, 0) &= ~0x00800000;
}

static inline void wake_publish2_mode1(void *unit, f32 x0, f32 y0, f32 z0, f32 x1, f32 y1, f32 z1, s32 tag)
{
    u32 flags;
    func_001072B8(unit);
    flags = WAKE_FIELD(unit, u32 *, 0);
    WAKE_FIELD(unit, u8 *, 0x91) = 0;
    WAKE_FIELD(unit, s32 *, 0x84) = -1;
    WAKE_FIELD(unit, s32 *, 0x80) = -1;
    WAKE_FIELD(unit, s32 *, 0x88) = 0;
    WAKE_FIELD(unit, u8 *, 0x92) = 0;
    WAKE_FIELD(unit, u32 *, 0) = flags & ~2;
    WAKE_FIELD(unit, f32 *, 0x28) = x0;
    WAKE_FIELD(unit, f32 *, 0x2c) = y0;
    WAKE_FIELD(unit, f32 *, 0x30) = z0;
    WAKE_FIELD(unit, s32 *, 0x58) = wake_grid_cell(x0, z0);
    WAKE_FIELD(unit, f32 *, 0x34) = x1;
    WAKE_FIELD(unit, f32 *, 0x38) = y1;
    WAKE_FIELD(unit, f32 *, 0x3c) = z1;
    WAKE_FIELD(unit, s32 *, 0x5c) = wake_grid_cell(x1, z1);
    WAKE_FIELD(unit, u8 *, 0x20) = 2;
    WAKE_FIELD(unit, u32 *, 0) &= ~4;
    WAKE_FIELD(unit, u32 *, 0) &= ~0x200;
    WAKE_FIELD(unit, u32 *, 0) |= 2;
    WAKE_FIELD(unit, u32 *, 0) &= ~0x00800000;
    WAKE_FIELD(unit, s32 *, 0x24) = tag;
    WAKE_FIELD(unit, u8 *, 0x91) = 1;
    WAKE_FIELD(unit, s32 *, 0x84) = -1;
}

static inline void wake_publish2_mode2(void *unit, f32 x0, f32 y0, f32 z0, f32 x1, f32 y1, f32 z1, s32 tag)
{
    u32 flags;
    func_001072B8(unit);
    flags = WAKE_FIELD(unit, u32 *, 0);
    WAKE_FIELD(unit, u8 *, 0x91) = 0;
    WAKE_FIELD(unit, s32 *, 0x84) = -1;
    WAKE_FIELD(unit, s32 *, 0x80) = -1;
    WAKE_FIELD(unit, s32 *, 0x88) = 0;
    WAKE_FIELD(unit, u8 *, 0x92) = 0;
    WAKE_FIELD(unit, u32 *, 0) = flags & ~2;
    WAKE_FIELD(unit, f32 *, 0x28) = x0;
    WAKE_FIELD(unit, f32 *, 0x2c) = y0;
    WAKE_FIELD(unit, f32 *, 0x30) = z0;
    WAKE_FIELD(unit, s32 *, 0x58) = wake_grid_cell(x0, z0);
    WAKE_FIELD(unit, f32 *, 0x34) = x1;
    WAKE_FIELD(unit, f32 *, 0x38) = y1;
    WAKE_FIELD(unit, f32 *, 0x3c) = z1;
    WAKE_FIELD(unit, s32 *, 0x5c) = wake_grid_cell(x1, z1);
    WAKE_FIELD(unit, u8 *, 0x20) = 2;
    WAKE_FIELD(unit, u32 *, 0) &= ~4;
    WAKE_FIELD(unit, u32 *, 0) &= ~0x200;
    WAKE_FIELD(unit, u32 *, 0) |= 2;
    WAKE_FIELD(unit, u32 *, 0) &= ~0x00800000;
    WAKE_FIELD(unit, s32 *, 0x24) = tag;
    WAKE_FIELD(unit, u8 *, 0x91) = 2;
    WAKE_FIELD(unit, s32 *, 0x84) = -1;
    WAKE_FIELD(unit, s32 *, 0x88) = 90;
}

static inline void wake_publish3_mode0(void *unit, f32 x0, u32 y0, f32 z0, f32 x1, u32 y1, f32 z1, f32 x2, u32 y2, f32 z2, s32 tag)
{
    u32 flags;
    func_001072B8(unit);
    flags = WAKE_FIELD(unit, u32 *, 0);
    WAKE_FIELD(unit, s32 *, 0x84) = -1;
    WAKE_FIELD(unit, s32 *, 0x80) = -1;
    WAKE_FIELD(unit, u8 *, 0x91) = 0;
    WAKE_FIELD(unit, s32 *, 0x88) = 0;
    WAKE_FIELD(unit, u8 *, 0x92) = 0;
    WAKE_FIELD(unit, u32 *, 0) = flags & ~2;
    WAKE_FIELD(unit, f32 *, 0x28) = x0;
    WAKE_FIELD(unit, u32 *, 0x2c) = y0;
    WAKE_FIELD(unit, f32 *, 0x30) = z0;
    WAKE_FIELD(unit, s32 *, 0x58) = wake_grid_cell(x0, z0);
    WAKE_FIELD(unit, f32 *, 0x34) = x1;
    WAKE_FIELD(unit, u32 *, 0x38) = y1;
    WAKE_FIELD(unit, f32 *, 0x3c) = z1;
    WAKE_FIELD(unit, s32 *, 0x5c) = wake_grid_cell(x1, z1);
    WAKE_FIELD(unit, f32 *, 0x40) = x2;
    WAKE_FIELD(unit, u32 *, 0x44) = y2;
    WAKE_FIELD(unit, f32 *, 0x48) = z2;
    WAKE_FIELD(unit, s32 *, 0x60) = wake_grid_cell(x2, z2);
    WAKE_FIELD(unit, u8 *, 0x20) = 3;
    WAKE_FIELD(unit, s32 *, 0x24) = tag;
    WAKE_FIELD(unit, u32 *, 0) &= ~4;
    WAKE_FIELD(unit, u32 *, 0) &= ~0x200;
    WAKE_FIELD(unit, u32 *, 0) |= 2;
    WAKE_FIELD(unit, u32 *, 0) &= ~0x00800000;
}

static inline void wake_publish3_mode1(void *unit, f32 x0, u32 y0, f32 z0, f32 x1, u32 y1, f32 z1, f32 x2, u32 y2, f32 z2, s32 tag)
{
    u32 flags;
    func_001072B8(unit);
    flags = WAKE_FIELD(unit, u32 *, 0);
    WAKE_FIELD(unit, u8 *, 0x91) = 0;
    WAKE_FIELD(unit, s32 *, 0x84) = -1;
    WAKE_FIELD(unit, s32 *, 0x80) = -1;
    WAKE_FIELD(unit, s32 *, 0x88) = 0;
    WAKE_FIELD(unit, u8 *, 0x92) = 0;
    WAKE_FIELD(unit, u32 *, 0) = flags & ~2;
    WAKE_FIELD(unit, f32 *, 0x28) = x0;
    WAKE_FIELD(unit, u32 *, 0x2c) = y0;
    WAKE_FIELD(unit, f32 *, 0x30) = z0;
    WAKE_FIELD(unit, s32 *, 0x58) = wake_grid_cell(x0, z0);
    WAKE_FIELD(unit, f32 *, 0x34) = x1;
    WAKE_FIELD(unit, u32 *, 0x38) = y1;
    WAKE_FIELD(unit, f32 *, 0x3c) = z1;
    WAKE_FIELD(unit, s32 *, 0x5c) = wake_grid_cell(x1, z1);
    WAKE_FIELD(unit, f32 *, 0x40) = x2;
    WAKE_FIELD(unit, u32 *, 0x44) = y2;
    WAKE_FIELD(unit, f32 *, 0x48) = z2;
    WAKE_FIELD(unit, s32 *, 0x60) = wake_grid_cell(x2, z2);
    WAKE_FIELD(unit, u8 *, 0x20) = 3;
    WAKE_FIELD(unit, s32 *, 0x24) = tag;
    WAKE_FIELD(unit, u32 *, 0) &= ~4;
    WAKE_FIELD(unit, u32 *, 0) &= ~0x200;
    WAKE_FIELD(unit, u32 *, 0) |= 2;
    WAKE_FIELD(unit, u32 *, 0) &= ~0x00800000;
    WAKE_FIELD(unit, u8 *, 0x91) = 1;
    WAKE_FIELD(unit, s32 *, 0x84) = -1;
}

static inline void wake_publish3_mode2(void *unit, f32 x0, u32 y0, f32 z0, f32 x1, u32 y1, f32 z1, f32 x2, u32 y2, f32 z2, s32 tag)
{
    u32 flags;
    func_001072B8(unit);
    flags = WAKE_FIELD(unit, u32 *, 0);
    WAKE_FIELD(unit, u8 *, 0x91) = 0;
    WAKE_FIELD(unit, s32 *, 0x84) = -1;
    WAKE_FIELD(unit, s32 *, 0x80) = -1;
    WAKE_FIELD(unit, s32 *, 0x88) = 0;
    WAKE_FIELD(unit, u8 *, 0x92) = 0;
    WAKE_FIELD(unit, u32 *, 0) = flags & ~2;
    WAKE_FIELD(unit, f32 *, 0x28) = x0;
    WAKE_FIELD(unit, u32 *, 0x2c) = y0;
    WAKE_FIELD(unit, f32 *, 0x30) = z0;
    WAKE_FIELD(unit, s32 *, 0x58) = wake_grid_cell(x0, z0);
    WAKE_FIELD(unit, f32 *, 0x34) = x1;
    WAKE_FIELD(unit, u32 *, 0x38) = y1;
    WAKE_FIELD(unit, f32 *, 0x3c) = z1;
    WAKE_FIELD(unit, s32 *, 0x5c) = wake_grid_cell(x1, z1);
    WAKE_FIELD(unit, f32 *, 0x40) = x2;
    WAKE_FIELD(unit, u32 *, 0x44) = y2;
    WAKE_FIELD(unit, f32 *, 0x48) = z2;
    WAKE_FIELD(unit, s32 *, 0x60) = wake_grid_cell(x2, z2);
    WAKE_FIELD(unit, u8 *, 0x20) = 3;
    WAKE_FIELD(unit, s32 *, 0x24) = tag;
    WAKE_FIELD(unit, u32 *, 0) &= ~4;
    WAKE_FIELD(unit, u32 *, 0) &= ~0x200;
    WAKE_FIELD(unit, u32 *, 0) |= 2;
    WAKE_FIELD(unit, u32 *, 0) &= ~0x00800000;
    WAKE_FIELD(unit, u8 *, 0x91) = 2;
    WAKE_FIELD(unit, s32 *, 0x84) = -1;
    WAKE_FIELD(unit, s32 *, 0x88) = 90;
}


/* Numeric offsets and the 12-byte point stride are observed in this owner. */
typedef struct WakePoint { f32 x, y, z; } WakePoint;
typedef struct WakeOwnerView {
    u8 bytes_00[0x20];
    u8 count;
    u8 bytes_21[7];
    WakePoint points[3];
    f32 backup_x, backup_y, backup_z;
    s32 cells[3];
    s32 backup_cell;
} WakeOwnerView;

void func_00121F38(RuntimeUnit *unit, WakeRecord18 *record) {
    f32 points[9];
    void *height_point;
    f32 *previous_point;
    f32 *first_point;
    f32 first_z;
    f32 first_z_before_nudge;
    f32 second_z_before_nudge;
    f32 third_z_before_nudge;
    f32 first_x;
    f32 first_z_after_nudge;
    f32 second_z_after_nudge;
    f32 third_z_after_nudge;
    s32 member_offset;
    s32 count;
    u8 first_value;
    u8 second_value;
    u8 third_value;
    u8 command_flag;
    u8 publication_flag;
    u8 kind;
    u8 mode;
    void *current_point;
    void *current_point_2;
    void *previous_point_2;
    void *second_point;
    void *third_point;

    kind = WAKE_FIELD(record, u8 *, 1);
    switch (kind) {
    case 0:
        WAKE_FIELD(unit, u8 *, 0x91) = (u8) WAKE_FIELD(record, u8 *, 2);
        command_flag = WAKE_FIELD(record, u8 *, 3);
        WAKE_FIELD(unit, u8 *, 0x92) = command_flag;
        if ((command_flag & 0xFF) == 2) {
            WAKE_FIELD(unit, u8 *, 0x92) = 0U;
            WAKE_FIELD(unit, s32 *, 0) = (s32) (WAKE_FIELD(unit, s32 *, 0) | 0x800000);
            return;
        }
        return;
    case 1:
        first_value = WAKE_FIELD(record, u8 *, 4);
        count = 0;
        if (first_value != 0) {
            count = 1;
            if (WAKE_FIELD(record, u8 *, 5) != 0) {
                first_x = D_801F0D98.field_00 + (((D_801F0D98.field_08 - D_801F0D98.field_00) * (f32)first_value) / 256.0f);
                points[0] = first_x;
                first_z = D_801F0D98.field_04 + (((D_801F0D98.field_0C - D_801F0D98.field_04) * (f32)WAKE_FIELD(record, u8 *, 5)) / 256.0f);
                points[2] = first_z;
                height_point = &points[0];
                {
                    /* One store after the call paths join keeps the destination
                       pointer live; the compiler duplicates it into jump delay slots. */
                    f32 height;
                    if (WAKE_FIELD(unit, s32 *, 0x70) == 1)
                        height = func_000F3428(D_800E7AB9, first_x, first_z);
                    else
                        height = func_000F315C(D_800E7AB9, first_x, first_z);
                    WAKE_FIELD(height_point, f32 *, 4) = height;
                }
            } else {
                func_0012EA80(first_value - 1, &points[0]);
            }
            first_point = &points[(count - 1) * 3];
            if (WAKE_FIELD(unit, f32 *, 8) == WAKE_FIELD(first_point, f32 *, 0)) {
                if (WAKE_FIELD(unit, f32 *, 0x10) == WAKE_FIELD(first_point, f32 *, 8)) {
                    first_z_before_nudge = WAKE_FIELD(first_point, f32 *, 8);
                    if (((f64) first_z_before_nudge + D_801EE610) < (f64) D_801F0D98.field_0C) {
                        first_z_after_nudge = 0.0001f;
                        first_z_after_nudge = first_z_before_nudge + first_z_after_nudge;
                    } else {
                        first_z_after_nudge = 0.0001f;
                        first_z_after_nudge = first_z_before_nudge - first_z_after_nudge;
                    }
                    WAKE_FIELD(first_point, f32 *, 8) = first_z_after_nudge;
                }
            }
            second_value = WAKE_FIELD(record, u8 *, 6);
            if (second_value != 0) {
                count++;
                if (WAKE_FIELD(record, u8 *, 7) != 0) {
                    second_point = &points[(count - 1) * 3];
                    WAKE_FIELD(second_point, f32 *, 0) = D_801F0D98.field_00 + (((D_801F0D98.field_08 - D_801F0D98.field_00) * (f32)second_value) / 256.0f);
                    WAKE_FIELD(second_point, f32 *, 8) = D_801F0D98.field_04 + (((D_801F0D98.field_0C - D_801F0D98.field_04) * (f32)WAKE_FIELD(record, u8 *, 7)) / 256.0f);
                    height_point = second_point;
                    {
                        f32 height;
                        if (WAKE_FIELD(unit, s32 *, 0x70) == 1)
                            height = func_000F3428(D_800E7AB9, points[3], points[5]);
                        else
                            height = func_000F315C(D_800E7AB9, points[3], points[5]);
                        WAKE_FIELD(height_point, f32 *, 4) = height;
                    }
                } else {
                    func_0012EA80(second_value - 1, &points[3]);
                }
                if (count == 1) {
                    if ((WAKE_FIELD(unit, f32 *, 8) == points[0]) && (WAKE_FIELD(unit, f32 *, 0x10) == points[2])) {
                        if (((f64) points[2] + D_801EE618) < (f64) D_801F0D98.field_0C) {
                            points[2] += 0.0001f;
                        } else {
                            points[2] -= 0.0001f;
                        }
                    }
                } else {
                    previous_point = &points[(count - 2) * 3];
                    current_point = &points[(count - 1) * 3];
                    if (WAKE_FIELD(previous_point, f32 *, 0) == WAKE_FIELD(current_point, f32 *, 0)) {
                        if (WAKE_FIELD(previous_point, f32 *, 8) == WAKE_FIELD(current_point, f32 *, 8)) {
                            second_z_before_nudge = WAKE_FIELD(current_point, f32 *, 8);
                            if (((f64) second_z_before_nudge + D_801EE620) < (f64) D_801F0D98.field_0C) {
                                second_z_after_nudge = 0.0001f;
                        second_z_after_nudge = second_z_before_nudge + second_z_after_nudge;
                            } else {
                                second_z_after_nudge = 0.0001f;
                        second_z_after_nudge = second_z_before_nudge - second_z_after_nudge;
                            }
                            WAKE_FIELD(current_point, f32 *, 8) = second_z_after_nudge;
                        }
                    }
                }
                third_value = WAKE_FIELD(record, u8 *, 8);
                if (third_value != 0) {
                    count++;
                    if (WAKE_FIELD(record, u8 *, 9) != 0) {
                        third_point = &points[(count - 1) * 3];
                        WAKE_FIELD(third_point, f32 *, 0) = D_801F0D98.field_00 + (((D_801F0D98.field_08 - D_801F0D98.field_00) * (f32)third_value) / 256.0f);
                        WAKE_FIELD(third_point, f32 *, 8) = D_801F0D98.field_04 + (((D_801F0D98.field_0C - D_801F0D98.field_04) * (f32)WAKE_FIELD(record, u8 *, 9)) / 256.0f);
                        height_point = third_point;
                    {
                        f32 height;
                        if (WAKE_FIELD(unit, s32 *, 0x70) == 1)
                            height = func_000F3428(D_800E7AB9, points[6], points[8]);
                        else
                            height = func_000F315C(D_800E7AB9, points[6], points[8]);
                        WAKE_FIELD(height_point, f32 *, 4) = height;
                    }
                    } else {
                        func_0012EA80(third_value - 1, &points[6]);
                    }
                    if (count == 1) {
                        if ((WAKE_FIELD(unit, f32 *, 8) == points[0]) && (WAKE_FIELD(unit, f32 *, 0x10) == points[2])) {
                            if (((f64) points[2] + D_801EE628) < (f64) D_801F0D98.field_0C) {
                                points[2] += 0.0001f;
                            } else {
                                points[2] -= 0.0001f;
                            }
                        }
                    } else {
                        previous_point_2 = &points[(count - 2) * 3];
                        current_point_2 = &points[(count - 1) * 3];
                        if (WAKE_FIELD(previous_point_2, f32 *, 0) == WAKE_FIELD(current_point_2, f32 *, 0)) {
                            if (WAKE_FIELD(previous_point_2, f32 *, 8) == WAKE_FIELD(current_point_2, f32 *, 8)) {
                                third_z_before_nudge = WAKE_FIELD(current_point_2, f32 *, 8);
                                if (((f64) third_z_before_nudge + D_801EE630) < (f64) D_801F0D98.field_0C) {
                                    third_z_after_nudge = 0.0001f;
                        third_z_after_nudge = third_z_before_nudge + third_z_after_nudge;
                                } else {
                                    third_z_after_nudge = 0.0001f;
                        third_z_after_nudge = third_z_before_nudge - third_z_after_nudge;
                                }
                                WAKE_FIELD(current_point_2, f32 *, 8) = third_z_after_nudge;
                            }
                        }
                    }
                }
            }
        }
        mode = WAKE_FIELD(record, u8 *, 2);
        switch (mode) {
        case 0:
            switch (count) {
            case 1:
                wake_publish1_mode0(unit, points[0], points[1], points[2], 1);
                break;
            case 2:
                wake_publish2_mode0(unit, points[0], points[1], points[2], points[3], points[4], points[5], 1);
                break;
            case 3:
                wake_publish3_mode0(unit, points[0], WAKE_FIELD(&points[1], u32 *, 0), points[2], points[3], WAKE_FIELD(&points[4], u32 *, 0), points[5], points[6], WAKE_FIELD(&points[7], u32 *, 0), points[8], 1);
                break;
            default:
                WAKE_FIELD(unit, u8 *, 0x91) = 0;
                break;
            }
            goto publication_done;
        case 1:
            switch (count) {
            case 1:
                wake_publish1_mode1(unit, points[0], points[1], points[2], 1);
                break;
            case 2:
                wake_publish2_mode1(unit, points[0], points[1], points[2], points[3], points[4], points[5], 1);
                break;
            case 3:
                wake_publish3_mode1(unit, points[0], WAKE_FIELD(&points[1], u32 *, 0), points[2], points[3], WAKE_FIELD(&points[4], u32 *, 0), points[5], points[6], WAKE_FIELD(&points[7], u32 *, 0), points[8], 1);
                break;
            default:
                WAKE_FIELD(unit, u8 *, 0x91) = 0;
                break;
            }
            goto publication_done;
        case 2:
            switch (count) {
            case 1:
                wake_publish1_mode2(unit, points[0], points[1], points[2], 1);
                break;
            case 2:
                wake_publish2_mode2(unit, points[0], points[1], points[2], points[3], points[4], points[5], 1);
                break;
            case 3:
                wake_publish3_mode2(unit, points[0], WAKE_FIELD(&points[1], u32 *, 0), points[2], points[3], WAKE_FIELD(&points[4], u32 *, 0), points[5], points[6], WAKE_FIELD(&points[7], u32 *, 0), points[8], 1);
                break;
            default:
                WAKE_FIELD(unit, u8 *, 0x91) = 0;
                break;
            }
            goto publication_done;
        publication_done:
            publication_flag = WAKE_FIELD(record, u8 *, 3);
            WAKE_FIELD(unit, u8 *, 0x92) = publication_flag;
            if ((publication_flag & 0xFF) == 2) {
                WAKE_FIELD(unit, u8 *, 0x92) = 0;
                WAKE_FIELD(unit, u32 *, 0) |= 0x800000;
            }
            break;
        }
        if (WAKE_FIELD(unit, s32 *, 0) & 0x800000) {
            ((WakeOwnerView *)unit)->backup_x = ((WakeOwnerView *)unit)->points[((WakeOwnerView *)unit)->count - 1].x;
            ((WakeOwnerView *)unit)->backup_y = ((WakeOwnerView *)unit)->points[((WakeOwnerView *)unit)->count - 1].y;
            ((WakeOwnerView *)unit)->backup_z = ((WakeOwnerView *)unit)->points[((WakeOwnerView *)unit)->count - 1].z;
            ((WakeOwnerView *)unit)->backup_cell = ((WakeOwnerView *)unit)->cells[((WakeOwnerView *)unit)->count - 1];
            return;
        }
        break;
    case 2:
        WAKE_FIELD(unit, s32 *, 0) = (s32) (WAKE_FIELD(unit, s32 *, 0) & ~1);
        member_offset = WAKE_FIELD(unit, u8 *, 4) * 0x19;
        D_801971F1[member_offset] = (s8) (D_801971F1[member_offset] & 0xFB);
        break;
    }
}
