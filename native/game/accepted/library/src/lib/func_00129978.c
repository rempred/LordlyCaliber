#include "common/types.h"
typedef struct Q7Unit Q7Unit;
/* Word-aligned copy extents observed in this body; no field meanings implied. */
typedef struct { u32 words[2400]; } Q7Copy2580;
typedef struct { u32 words[184]; } Q7Copy2E0;
typedef struct { u32 words[22]; } Q7Copy58;
extern Q7Copy2580 D_801F1080, D_801910F0;
extern Q7Copy2E0 D_801F0D90, D_801936E8;
extern Q7Copy58 D_80193B60;
extern Q7Copy58 *D_8018F58C;
extern s32 D_801951A8, D_801F367C;
extern u8 D_801971F0[], D_801971F1[], D_801977F6[];
extern u8 D_801F1038, D_801F365D;
extern Q7Unit *D_801F0CB0[];
extern void func_0012967C(s32);
extern void func_00128E80(void);
extern s32 func_80186210(s32);
extern void func_00129268(void);
extern void func_0012FE58(s32);

static __inline__ void restore_first_blocks(void)
{
    D_801F1080 = D_801910F0;
    D_801F0D90 = D_801936E8;
}

static __inline__ void restore_last_block(void)
{
    D_801F367C = D_801951A8;
    *D_8018F58C = D_80193B60;
}

static __inline__ void select_records(void)
{
    s32 index = 0;
    u8 *unit = (u8 *)&D_801F1080;
    u8 *metadata = D_801971F0;
    for (; index < 50; ++index) {
        if (!(metadata[1] & 4)) {
            if (index < 30 || !(*(u32 *)unit & 0x11))
                func_0012967C(index);
        }
        unit += 0xC0;
        metadata += 25;
    }
}

static __inline__ void update_flag(void)
{
    if (func_80186210(18))
        D_801F365D |= 2;
    else
        D_801F365D &= 0xFD;
}

void func_00129978(s32 mode)
{
    s32 index;
    s32 count;
    /* Preserve the eight unused bytes in the original 40-byte frame. */
    volatile u32 frame_pad[2];
    s32 metadata_offset;
    u8 *mapping;
    Q7Unit **unit_slot;
    u8 *unit;

    if (mode == 0) {
        for (index = 0; index < 50; ++index)
            func_0012967C(index);
        D_801F367C = index;
        goto finish_restore;
    }
    if (mode == 1) {
        restore_first_blocks();
        restore_last_block();
        select_records();
        goto finish_restore;
    }
    if ((mode == 2) | (mode == 5)) {
        restore_first_blocks();
        restore_last_block();
        select_records();
        func_00128E80();
        update_flag();
        goto finalize;
    }
    if (mode == 3) {
        restore_first_blocks();
        restore_last_block();
        select_records();
        func_00128E80();
        update_flag();
        goto finalize;
    }
    if (mode != -1)
        goto finalize;
    restore_first_blocks();
    D_801F367C = D_801951A8;
    if (D_801F1038 == 0)
        *D_8018F58C = D_80193B60;
    select_records();

finish_restore:
    func_00128E80();

finalize:
    count = D_801F367C;
    index = 0;
    if (count > 0) {
        unit_slot = D_801F0CB0;
        unit = (u8 *)&D_801F1080;
        do {
            *unit_slot++ = (Q7Unit *)unit;
            unit += 0xC0;
            ++index;
        } while (index < count);
    }
    index = 30;
    count = 0;
    mapping = D_801977F6;
    metadata_offset = 30 * 25;
    do {
        u32 flags = D_801971F1[metadata_offset];
        /* Keep the two flag tests as separate guards. */
        if (flags & 1) {
            if (!(flags & 0x80)) {
                *mapping = count + 30;
                ++count;
            } else {
                *mapping = 0;
            }
        } else {
            *mapping = 0;
        }
        ++mapping;
        metadata_offset += 25;
        ++index;
    } while (index < 50);
    index = 30;
    {
        /* This scan has a separate pointer lifetime from the first map pass. */
        u8 *remaining = D_801977F6;
        for (; index < 50; ++index) {
            if (*remaining == 0)
                *remaining = index + count;
            ++remaining;
        }
    }
    func_00129268();
    func_0012FE58(2);
}
