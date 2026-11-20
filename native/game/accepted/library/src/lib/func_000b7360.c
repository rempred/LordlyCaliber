typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
extern u8 *D_80196AF8;
u8 func_000bd26c(u16 row);
void func_000bafa0(void);
void func_000bb0cc(void);
void func_000badd4(void);
void func_000bacec(void);
void func_000b7360(void) {
    s32 i;
    s32 k;
    u8 *row;
    u8 *rec;
    u8 u;
    for (i = 0; i < 10; i++) {
        row = D_80196AF8 + 0x10D4 + i * 14;
        if (row[8] == 0) {
            continue;
        }
        for (k = 0; k < 5; k++) {
            u = (row + k)[2];
            if (u == 0xFF) {
                continue;
            }
            rec = D_80196AF8 + (u * 54 + 0x117C);
            if (!(rec[1] & 8)) {
                (*(s32 *)((u8 *)(D_80196AF8) + (0x2C0)))--;
                rec[1] |= 8;
            }
        }
        row[0xA] = func_000bd26c(i);
    }
    func_000bafa0();
    func_000bb0cc();
    func_000badd4();
    func_000bacec();
}
