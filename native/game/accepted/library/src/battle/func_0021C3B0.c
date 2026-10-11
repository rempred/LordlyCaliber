typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;

extern u8 D_80190F80[];
extern u8 D_80193BC0[];
extern u8 D_801953F0[];
extern u8 D_80195560[];
extern u8 D_801976DC;
extern u8 D_801976E8;

void *func_0020C478(s32 index);
s32 func_801D0880(void);
s32 func_0020BFF8(void *actor);
s32 func_0020C0E8(void *actor);
s32 func_0020C32C(void *actor);
s32 func_0020C0CC(void *actor);
s32 func_0020C014(void *actor);
s32 func_0020C2C0(void *actor);

void func_0021C3B0(void)
{
    s32 index;
    u8 *actor;
    u8 *record;
    u8 *table_base;
    s32 side;
    u8 actor_id;
    u8 *flag;

    for (index = 0; index < 20; index++) {
        actor = func_0020C478(index);
        if (func_801D0880() != 0 &&
            func_0020BFF8(actor) != 0 &&
            func_0020C0E8(actor) != 0 &&
            func_0020C32C(actor) != 0) {
            *(u16 *)(actor + 0x20) = 0;
            *(s32 *)(actor + 0x40) &= ~2;
        }

        if (actor != 0 && func_0020C0CC(actor) == 0) {
            if (func_0020C014(actor) != 0) {
                side = D_801976DC;
            } else {
                side = D_801976E8;
            }

            if (side < 30) {
                table_base = D_80190F80;
                record = D_80193BC0 + actor[0xF6] * 0x38;
            } else {
                table_base = D_801953F0;
                record = D_80195560 + actor[0xF6] * 0x34;
            }

            actor_id = actor[0xF6];
            if (actor_id == 0xFF) {
                continue;
            }

            if (actor_id < 100) {
                *(u16 *)(record + 0x18) = *(u16 *)(actor + 0x20);
                record[0x1B] = actor[0x34];
                if (func_0020C32C(actor) != 0) {
                    record[0x33] |= 4;
                } else {
                    record[0x33] &= 0xFB;
                }
                if (side < 30) {
                    record[0x35] = actor[0x32];
                }
            } else {
                flag = table_base;
                flag += actor_id * 2;
                *(u16 *)(flag - 0xC4) = *(u16 *)(actor + 0x20);
                if (func_0020C2C0(actor) == 0 &&
                    func_0020C32C(actor) != 0) {
                    flag = table_base + actor[0xF6] + 0x90;
                    *flag |= 4;
                } else {
                    flag = table_base + actor[0xF6] + 0x90;
                    *flag &= 0xFB;
                }
            }
        }
    }
}
