typedef unsigned char u8;
typedef unsigned short u16;
typedef struct {
    u8 pad[0x12];
    u8 field12;
    u8 pad13[5];
    u16 field18;
    u8 tail[30];
} Record56;
typedef struct {
    u8 pad[0x12];
    u8 field12;
    u8 pad13[5];
    u16 field18;
    u8 tail[26];
} Record52;
typedef struct {
    u8 field0;
    u8 field1;
    u8 members[5];
    u8 tail[18];
} Record25;

extern Record56 g_func_0019554C_records_56[];
extern Record52 g_func_0019554C_records_52[];
extern Record25 g_func_001957D0_source_records[];
extern u8 D_801F0BD8[];
extern u8 D_801F0C3C;
extern void func_80093380(void *, int);

void func_00128E80(void)
{
    int i;
    int j;
    int value;
    int k;

    func_80093380(D_801F0BD8, 200);
    D_801F0BD8[0] |= 1;
    D_801F0C3C |= 1;
    for (i = 1; i < 100; i++) {
        if (g_func_0019554C_records_56[i].field12 == 0)
            D_801F0BD8[i] |= 1;
        if (g_func_0019554C_records_56[i].field18 == 0)
            D_801F0BD8[i] |= 1;
    }
    for (i = 1; i < 100; i++) {
        if (g_func_0019554C_records_52[i].field12 == 0)
            /* This address form preserves the retail indexed symbol load. */
            (*(u8 *)((unsigned int)&D_801F0C3C + i)) |= 1;
        if (g_func_0019554C_records_52[i].field18 == 0)
            (*(u8 *)((unsigned int)&D_801F0C3C + i)) |= 1;
    }
    for (k = 0; k < 50; k++) {
        if (g_func_001957D0_source_records[k].field1 & 1) {
            if (k < 30) {
                for (j = 0; j < 5; j++) {
                    /* The pinned compiler needs the explicit second member read. */
                    value = g_func_001957D0_source_records[k].members[j];
                    if (value != 0 && (value = g_func_001957D0_source_records[k].members[j]) < 100)
                        D_801F0BD8[value] |= 1;
                }
            } else {
                for (j = 0; j < 5; j++) {
                    value = g_func_001957D0_source_records[k].members[j];
                    if (value != 0 && (value = g_func_001957D0_source_records[k].members[j]) < 100)
                        (*(u8 *)((unsigned int)&D_801F0C3C + value)) |= 1;
                }
            }
        }
    }
}
