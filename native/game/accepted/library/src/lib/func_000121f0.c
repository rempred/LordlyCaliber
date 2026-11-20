typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    u32 base_u;
    s32 len;
    u8 type;
    u8 flags;
    u32 loop;
    u32 book;
} MusWave;
typedef struct {
    u8 pad00[0x10];
    s32 flags;
    u8 pad14[0xC];
    s32 count;
    u8 *basenote;
    u8 *detune;
    MusWave **waves;
} MusBank;
extern void *D_800B1828;
MusBank *func_000121f0(MusBank *bank) {
    if (bank != ((void *)0) && bank->flags < 0) {
        D_800B1828 = bank;
    }
    return D_800B1828;
}
