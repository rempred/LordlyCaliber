#include "common/types.h"

extern u32 D_8021A010[50];
extern void *D_80219F48[50];
extern void *D_8021A128, *D_8021A12C, *D_8021A130;
extern u8 D_8021A134[];
extern s32 D_801F3658;
extern u8 D_801971F2[][25];
extern u8 D_80193BD1[][56];

extern void *resource_alloc(s32 size);
extern void resource_free(void *resource);
extern u8 *func_0002E138(u32 key);
extern void func_00023460(const void *source, void *destination, u32 size);
extern void *func_8016FBE0(u32 field, s32 mode, void *output);

void func_0019BB34(void)
{
    s32 index = 49;
    void **clear_cursor = &D_80219F48[49];

    for (; index >= 0; index--) {
        *clear_cursor = 0;
        clear_cursor--;
    }

    if (D_8021A010[0] != 0) {
        u8 *bank = func_0002E138(0x01DEE82C);
        u32 *equipment_cursor;
        void **output_cursor;

        index = 0;
        output_cursor = D_80219F48;
        equipment_cursor = D_8021A010;

        for (; index < 50; index++) {
            u8 *resource;
            if (*equipment_cursor == 0) {
                break;
            }
            resource = resource_alloc(0x300);
            func_00023460(bank, resource, 0x200);
            /* Retail reloads the ID after the first copy, then advances. */
            func_00023460(bank + 0x100 + (*equipment_cursor << 8),
                          resource + 0x200, 0x100);
            equipment_cursor++;
            *output_cursor++ = resource;
        }
        resource_free(bank);
    }

    D_8021A128 = func_0002E138(0x01DC2EE8);
    D_8021A12C = func_0002E138(0x01DCBFC2);
    D_8021A130 = func_8016FBE0(
        D_80193BD1[D_801971F2[D_801F3658][0]][0], 0, D_8021A134);
}
