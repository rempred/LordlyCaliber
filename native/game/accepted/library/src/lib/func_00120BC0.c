typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;

typedef struct RuntimeUnit {
    u32 unk_00;
    u32 unk_04;
    f32 unk_08, unk_0C, unk_10;
    s32 unk_14;
    f32 unk_18, unk_1C;
    u8 unk_20;
    u8 pad_21[3];
    s32 unk_24;
    u8 pad_28[0x24];
    f32 unk_4C, unk_50, unk_54;
    u8 pad_58[0xC];
    s32 unk_64;
    u8 pad_68[0x18];
    s32 unk_80, unk_84;
    u8 pad_88[9];
    u8 unk_91, unk_92;
} RuntimeUnit;
/* Array views retain the separate global loads made by the retail compiler. */
extern f32 D_801F0D98[];
extern f32 D_801F0D9C[];
extern f32 D_801F0DA0[];
extern f32 D_801F0DA4[];
extern void func_00129948(RuntimeUnit *unit);

/* Complete logical body; original four-byte physical lead stays in ASM. */
void func_00120BC0(RuntimeUnit *unit, f32 arg1, f32 arg2, f32 arg3, s32 arg4)
{
    unit->unk_0C = arg2;
    unit->unk_08 = arg1;
    unit->unk_10 = arg3;
    /* Reuse the float arguments so KMC preserves the native calculation order. */
    arg3 -= D_801F0D9C[0];
    arg3 *= 64.0f;
    arg3 /= D_801F0DA4[0] - D_801F0D9C[0];
    arg1 -= D_801F0D98[0];
    arg1 *= 64.0f;
    arg1 /= D_801F0DA0[0] - D_801F0D98[0];
    unit->unk_14 = ((s32)arg3 << 6) + (s32)arg1;
    unit->unk_4C = unit->unk_08;
    unit->unk_50 = unit->unk_0C;
    unit->unk_54 = unit->unk_10;
    unit->unk_64 = unit->unk_14;
    unit->unk_18 = 0.75f;
    unit->unk_1C = 0.75f;
    unit->unk_00 |= 1;
    func_00129948(unit);
    unit->unk_84 = -1;
    unit->unk_80 = -1;
    unit->unk_91 = 0;
    unit->unk_92 = 0;
    unit->unk_20 = 0;
    unit->unk_24 = 0;
    unit->unk_00 &= ~4;
    unit->unk_00 &= ~0x200;
    if (arg4) {
        /* The final conditional flag update reloads the word in retail. */
        ((volatile RuntimeUnit *)unit)->unk_00 |= 0x00800000;
    }
}





