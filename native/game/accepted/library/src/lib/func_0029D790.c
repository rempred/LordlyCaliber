typedef signed char s8;
typedef unsigned char u8;
typedef signed int s32;
typedef float f32;

typedef struct Func0029D790Manager {
    u8 pad_00[0x18];
    void *records[1];
} Func0029D790Manager;

extern Func0029D790Manager * volatile D_8022A974;
extern s32 D_801CE8FC;

void *func_00001330(s32 size);
void func_00023780(void *destination, s32 size);
void func_8022E3DC(void **record, s32 arg1, s32 arg2, s32 arg3,
                   f32 arg4, f32 arg5, f32 arg6, s32 arg7, s32 arg8,
                   s32 zero, s32 slot, s32 one);
void func_8022E9E8(void *record);

void func_0029D790(s32 arg0, s32 arg1, s32 arg2, s32 arg3,
                   f32 arg4, f32 arg5, f32 arg6, u8 arg7, u8 arg8)
{
    void *record;
    if (D_8022A974->records[arg0] == 0) {
        D_8022A974->records[arg0] = func_00001330(0x150);
    }
    record = D_8022A974->records[arg0];
    func_00023780(record, 0x150);
    *((s8 *)record + 0x13C) = D_801CE8FC;
    func_8022E3DC(&record, arg1, arg2, arg3, arg4, arg5, arg6,
                  arg7, arg8, 0, arg0 & 0xFF, 1);
    func_8022E9E8(record);
}
