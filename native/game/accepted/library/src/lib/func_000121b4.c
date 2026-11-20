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
extern MusBank *D_800B182C;
void func_000121b4(MusBank *bank, u8 *wbank);
void func_0001390c(MusBank *pbank, u8 *wbank);
void func_000121b4(MusBank *bank, u8 *wbank) {
    func_0001390c(bank, wbank);
    if (D_800B182C == ((void *)0)) {
        D_800B182C = bank;
    }
}
