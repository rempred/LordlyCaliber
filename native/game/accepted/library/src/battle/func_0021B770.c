typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;

s32 func_0020C014(void *);
s32 func_0020C0CC(void *);
s32 func_0020C104(void *);
s32 func_0020C2C0(void *);
s32 func_0020C32C(void *);
void *func_0020C478(s32);

extern u8 D_80193BC0[];
extern u8 D_80195560[];

void func_0021B770(void)
{
    s32 i;
    s32 table_count;
    s32 use_first_table;
    void *actor;
    u8 *record;

    i = 0;
    do {
        actor = func_0020C478(i);
        if ((actor != 0) &&
            (func_0020C104(actor) == 0) &&
            (func_0020C0CC(actor) == 0) &&
            ((func_0020C2C0(actor) != 0) ||
             (func_0020C32C(actor) != 0) ||
            (*(u16 *)((s8 *)actor + 0x20) == 0))) {
            if (func_0020C014(actor) != 0) {
                table_count = *(u8 *)0x801976DC;
                use_first_table = table_count < 0x1E;
            } else {
                table_count = *(u8 *)0x801976E8;
                use_first_table = table_count < 0x1E;
            }

            if (use_first_table != 0) {
                record = (*(u8 *)((s8 *)actor + 0xF6) * 0x38) + D_80193BC0;
            } else {
                record = (*(u8 *)((s8 *)actor + 0xF6) * 0x34) + D_80195560;
            }
            record[0x32] = 0;
        }
        i++;
    } while (i < 0x14);
}
