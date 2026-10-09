typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct RuntimeUnit RuntimeUnit;

extern RuntimeUnit *D_801F0CB0[];
extern s32 D_801F367C;
extern u8 D_80195560[];
extern u16 D_801953F4[120];
extern u8 D_801974DE[];
extern u8 D_801969B8[];
extern u8 D_801969B9[];
extern void func_0010746C(RuntimeUnit *unit);
extern void resource_free(void *resource);
extern void memset_00023780(void *destination, s32 bytes);

void func_0012A050(void)
{
    s32 index;
    u8 *record;
    u8 *flag;
    s32 offset;
    u32 address;
    u8 *last_record;

    for (index = 0; index < D_801F367C; index++) {
        RuntimeUnit *unit = D_801F0CB0[index];
        void *fieldA8;

        func_0010746C(unit);
        fieldA8 = *(void **)((u8 *)unit + 0xA8);
        if (fieldA8 != 0) {
            resource_free(fieldA8);
        }
        memset_00023780(unit, 0xC0);
    }

    index = 1;
    /* An integer base preserves the shared address before both byte offsets.
       Pointer CSE instead folds the base into a cursor and drops a retail instruction. */
    address = (u32)D_80195560;
    flag = (u8 *)(address + 0x67);
    record = (u8 *)(address + 0x34);
    for (; index < 99; index++) {
        memset_00023780(record, 0x34);
        record += 0x34;
        /* Keep the flag publication after the clear call, as in the ROM. */
        *flag &= 0x7F;
        flag += 0x34;
    }

    for (index = 119; index >= 0; index--) {
        D_801953F4[index] = 0;
    }

    index = 30;
    {
        u8 *rows25 = D_801974DE;
        for (; index < 50; index++) {
            memset_00023780(rows25, 25);
            rows25 += 25;
        }
    }

    index = 0;
    last_record = D_801969B8;
    offset = 0;
    for (; index < 10; index++, offset += 11) {
        if (D_801969B9[offset] & 0x80) {
            memset_00023780(last_record, 11);
        }
        last_record += 11;
    }
}
