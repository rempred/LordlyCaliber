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

s32 func_00123E38(s32 source_word, s32 member)
{
    s32 eligible = 0;
    s32 index;
    s32 count;
    u8 *record;

    /* The aligned base preserves the retail indexed field loads and stores. */
    if (member < 100) {
        u8 *row = (u8 *)0x80190000 + member * 52;
        if (*(u16 *)(row + 0x5578) != 0) {
            if ((row[0x5593] & 4) != 0)
                eligible = 1;
        }
    } else {
        u8 *row = (u8 *)0x80190000;
        if (*(u16 *)(row + member * 2 + 0x532C) != 0) {
            if ((row[member + 0x5480] & 4) != 0)
                eligible = 1;
        }
    }

    if (eligible != 0) {
        count = (u16)func_0014F300((u8)source_word);
        record = D_801971F0[source_word];
        for (index = 0; index < count; ++index) {
            u8 *entry = record + index;
            if (entry[13] == 6) {
                if (member < 100) {
                    u8 *row = (u8 *)0x80190000 + member * 52;
                    row[0x5593] &= 0xFB;
                } else {
                    u8 *row = (u8 *)0x80190000;
                    row[member + 0x5480] &= 0xFB;
                }
                /* Separate used pointer lifetimes retain the retail induction. */
                {
                    u8 *clear_entry = record + index;
                    clear_entry[13] = 0;
                }
                func_00131388(record);
                if ((*(u32 *)D_801F0CB0[source_word] & 0x20) != 0) {
                    func_00131050();
                    func_00147FB8(6, source_word);
                    func_0015DF10(19);
                }
                break;
            }
        }
        /* Retain the shared result register; exhaustion returns zero. */
        eligible &= -(index != count);
    }
    return eligible;
}
