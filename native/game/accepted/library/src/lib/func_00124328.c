typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef struct RuntimeUnit RuntimeUnit;

extern u8 D_801971F0[][25];
extern RuntimeUnit *D_801F0CB0[];
extern u32 func_0014F300(s32 source_word);
extern void func_00131388(void *record);
extern s32 func_00131050();
extern void func_00147FB8(int unused_first, int source_word);
extern void func_0015DF10(u32 value);

s32 func_00124328(s32 source_word, s32 member)
{
    s32 eligible = 0;
    s32 amount;
    s32 index;
    s32 count;
    u8 *record;

    if (member < 100) {
        u8 *row = (u8 *)0x80190000 + member * 52;
        if (*(u16 *)(row + 0x5578) != 0)
            eligible = row[0x5592] >= 70;
    }

    if (eligible != 0) {
        count = (u16)func_0014F300((u8)source_word);
        amount = 0;
        record = D_801971F0[source_word];
        for (index = 0; index < count; ++index) {
            u8 *entry = record + index;
            s32 value = entry[13];
            if (value == 4)
                amount = 20;
            else if (value == 5)
                amount = 50;
            if (amount != 0) {
                s32 slot;
                /* Keep the two actual pointer uses in separate lifetimes. */
                {
                    u8 *clear_entry = record + index;
                    clear_entry[13] = 0;
                }
                func_00131388(record);
                for (slot = 0; slot < 5; ++slot) {
                    /* The initial member is dead; reuse its word variable to
                     * retain the retail argument-register preference. */
                    member = (record + slot)[2];
                    if (member != 0) {
                        if (member < 100) {
                            u8 *row = (u8 *)0x80190000 + member * 52;
                            if (*(u16 *)(row + 0x5578) != 0) {
                                s32 before = row[0x5592];
                                if (before >= amount)
                                    row[0x5592] = before - amount;
                                else
                                    row[0x5592] = 0;
                            }
                        }
                    }
                }
                break;
            }
        }
        if (index == count) {
            eligible = 0;
        } else {
            if ((*(u32 *)D_801F0CB0[source_word] & 0x20) != 0) {
                func_00131050();
                func_00147FB8(4, source_word);
                func_0015DF10(19);
            }
        }
    }
    return eligible;
}
