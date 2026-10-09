typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;

typedef struct { u32 flags; u8 source_index; } RuntimeUnit;
typedef struct { u8 bytes[25]; } SourceRecord25;
typedef struct { u8 bytes[52]; } MemberRecord52;

extern SourceRecord25 D_801971F0[];
extern MemberRecord52 D_80195560[];
extern u16 D_8019532C[];
extern u8 D_80195480[];
extern RuntimeUnit *D_801F0CB0[];
extern void func_80093380(void *destination, s32 bytes);
extern void func_0010746C(RuntimeUnit *unit);

void func_0012A828(s32 source_index)
{
    SourceRecord25 *record = &D_801971F0[source_index];
    RuntimeUnit *unit = D_801F0CB0[source_index];
    s32 slot;

    if (record->bytes[1] & 1) {
        for (slot = 0; slot < 5; slot++) {
            s32 member = record->bytes[slot + 2];
            if (member != 0) {
                if (member >= 100) {
                    D_8019532C[member] = 0;
                    D_80195480[member] = 0;
                } else {
                    func_80093380(&D_80195560[member], 52);
                }
            }
        }
        func_80093380(record, 25);
    }
    record->bytes[1] = 0x80;
    func_0010746C(unit);
    unit->flags &= ~1;
    unit->flags &= ~0x10;
    unit->flags &= ~8;
}
