#include "game/resource_arena.h"

extern char D_800ADD00[];
extern char D_800ADD38[];
extern char D_800ADD70[];
extern void func_80093540(const char *, ...);
extern void func_80093380(void *, s32);

s32 resource_arena_register(u32 start, u32 size)
{
    u32 end = size;
    s32 i;
    ArenaNode **base_cursor;
    ArenaNode **end_cursor;
    s32 n;
    s32 slot;
    ArenaNode *end_pointer;

    if (D_800AEDE0 >= 4) {
        func_80093540(D_800ADD00, start, size);
        for (;;) {}
    }
    end = (start + end) & -16;
    start = (start + 15) & -16;
    /* The live loop comparison preserves the signed entry guard and the
     * compiler's comparison home in the 48-byte frame. */
    i = 0;
    if (i < D_800AEDE0) {
        end_cursor = &D_800AEDB0[0].end;
        base_cursor = end_cursor - 1;
        do {
            if ((start >= (u32)*base_cursor && start < (u32)*end_cursor) ||
                ((u32)*base_cursor < end && end <= (u32)*end_cursor)) {
                func_80093540(D_800ADD38, start, end);
                func_80093540(D_800ADD70, i, *base_cursor, *end_cursor);
                for (;;) {}
            }
            end_cursor += 3;
            ++i;
            base_cursor += 3;
        } while (i < D_800AEDE0);
    }
    D_800AEDB0[D_800AEDE0].base = (ArenaNode *)start;
    func_80093380((void *)start, 0x20);
    D_800AEDB0[D_800AEDE0].base->next = (ArenaNode *)end;
    D_800AEDB0[D_800AEDE0].base->available = end - start - 0x20;
    D_800AEDB0[D_800AEDE0].root = 0;
    func_00001DE8(&D_800AEDB0[D_800AEDE0].root,
                 D_800AEDB0[D_800AEDE0].base);
    n = D_800AEDE0;
    /* Capture the old slot before changing the local count. This keeps the
     * narrow-index mask that GCC otherwise combines into the count load. */
    slot = (u16)n;
    n++;
    end_pointer = D_800AEDB0[slot].base->next;
    D_800AEDE0 = n;
    D_800AEDB0[slot].end = end_pointer;
    /* Retail retains this old slot in v0 through JR. Returning it keeps that
     * value live and preserves the tail's temporary-register allocation. */
    return slot;
}

/* Required independently callable leaf at owner offset 0x1F4. */
static void func_00001314(void)
{
    D_800AEDE0 = 1;
    D_800AEDE2 = 3;
}
