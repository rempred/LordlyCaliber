typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u8 used;
    u8 value;
    u16 id;
} ScriptSlot;
extern ScriptSlot D_8022A1F0[];
extern u8 *D_8022A248;
extern s32 D_8022A24C;
extern volatile s32 D_8022A250;
extern s32 *D_8022A214;
extern s32 D_8022A82C;
extern u16 D_8022A254;
extern u16 D_8022A256;
extern u16 D_8022A218[];
extern u16 D_8022A822;
extern u16 D_8022A830;
extern u16 D_8022A828;
extern u8 *D_8022A234;
extern u8 D_801936E0;
extern u8 D_80196AEC;
void bzero(void *p, s32 size);
extern s32 D_8022A210;
extern s32 D_8022A230;
void bzero(void *p, s32 n);
typedef struct {
    u8 *vram;
    s32 unk04;
    u8 *romStart;
    u8 *romEnd;
    u8 *bssStart;
    u8 *bssEnd;
    u8 *textStart;
    u8 *textEnd;
    u8 *dataStart;
    u8 *dataEnd;
} OvlDesc;
typedef struct {
    s32 a;
    s32 b;
    void (*init)(void);
    s32 c;
    s32 d;
    u8 *e;
    s32 f;
} EvSetup;
extern OvlDesc D_80229DBC[];
extern EvSetup D_80229E68[];
extern s32 D_8022A238;
extern s32 D_8022A23C;
extern s32 D_8022A240;
extern s32 D_8022A244;
void os_inval_icache(void *p, s32 n);
void osInvalDCache(void *p, s32 n);
void D_8009DA50(void *rom, void *dst, s32 size);
void func_001BDD70(void);
void func_001BDE70(void);
void func_001C2A6C(void);
extern inline void func_001BA074(u16 id, u8 v) {
    s32 i;
    for (i = 0; i < 8; i++) {
        if (D_8022A1F0[i].used == 0) {
            break;
        }
    }
    if (i >= 8) {
        for (;;) {
        }
    }
    D_8022A1F0[i].value = v;
    D_8022A1F0[i].id = id;
    D_8022A1F0[i].used = 1;
}
extern inline void func_001BA37C(void) {
    if (D_8022A248 != ((void *)0) && D_8022A24C != 0) {
        bzero(D_8022A248, D_8022A24C);
    }
    D_8022A250 = 0;
}
void func_001BAE48(void) {
    OvlDesc *o;
    EvSetup *e;
    if (D_801936E0 == 0) {
        return;
    }
    D_8022A230 = 8;
    D_8022A234 = func_00001330(0x1C0);
    bzero(D_8022A234, 0x1C0);
    D_8022A82C = 0;
    bzero(D_8022A218, 0x18);
    D_8022A254 = 0;
    D_8022A256 = 0;
    D_8022A828 = 0;
    D_8022A830 = 0;
    D_8022A822 = 0;
    bzero(D_8022A1F0, 0x20);
    o = &D_80229DBC[D_801936E0];
    D_8022A214 = ((void *)0);
    D_8022A210 = -1;
    if (o->vram != ((void *)0)) {
        os_inval_icache(o->textStart, o->textEnd - o->textStart);
        osInvalDCache(o->dataStart, o->dataEnd - o->dataStart);
        D_8009DA50(o->romStart, o->vram, o->romEnd - o->romStart);
        if (o->bssEnd != o->bssStart) {
            bzero(o->bssStart, o->bssEnd - o->bssStart);
        }
    }
    e = &D_80229E68[D_801936E0];
    D_8022A238 = e->a;
    D_8022A23C = e->b;
    if (e->init != ((void *)0)) {
        e->init();
    }
    D_8022A240 = e->c;
    D_8022A244 = e->d;
    D_8022A248 = e->e;
    D_8022A24C = e->f;
    func_001BDD70();
    func_001BA074(0x5100, 0xFF);
    func_001BDE70();
    func_001C156C();
    func_001C2064();
    func_001C2A6C();
    func_001BA074(0x5500, 0);
    func_001BA37C();
    D_80196AEC |= 0x80;
}
