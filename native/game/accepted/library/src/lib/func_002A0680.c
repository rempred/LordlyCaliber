typedef signed char s8;
typedef unsigned char u8;

typedef struct {
    u8 pad_0000[0x1CB1];
    u8 alternate_scheduler;
} Func002A0680Scene;

extern Func002A0680Scene *D_8022A974;
extern u8 D_8018FC19;
extern s8 D_801CFC70;

void func_80227694(void);
void func_802282B8(void);
void func_8022CBD4(void);
void func_8023C71C(void);
void func_8023EAD0(void);
void func_8023A7F4(void);
void func_802401B4(void);
void func_80241610(void);
void func_8024049C(void);
void func_8023BDDC(void);
void func_8022AE84(void);
void func_802316C0(void);
void func_8022CF60(void);
void func_80230FD4(void);
void func_8023E354(void);
void func_801C3D64(void);
void func_80239AF8(void);
void func_8023DAC4(void);
void func_80230B60(void);
void func_802345E0(void);
void func_80234770(void);
void func_80178E30(void);

void func_002A0680(void)
{
    int mode;
    register int mode_two asm("$16");

    if (D_8022A974->alternate_scheduler != 0) {
        func_80227694();
        func_802282B8();
        asm(".set noreorder\n.word 0x0808C3F1\nnop\n.set reorder");
    }

    func_8022CBD4();
    mode = D_8018FC19;
    asm(".set noreorder\nbne %1,$0,.Lfunc_002A0680_mode_done\nli %0,2\n.set reorder"
        : "=r"(mode_two)
        : "r"(mode));
    func_8023C71C();
    func_8023EAD0();
    func_8023A7F4();
    func_802401B4();
    func_80241610();
    func_8024049C();
    mode = D_8018FC19;
    asm(".Lfunc_002A0680_mode_done:");
    if (mode == mode_two) {
        func_8023BDDC();
    }

    func_80227694();
    func_8022AE84();
    func_802316C0();
    func_8022CF60();
    func_80230FD4();

    mode = D_8018FC19;
    if (mode == mode_two) {
        func_8023E354();
        func_801C3D64();
        func_80239AF8();
        func_8023DAC4();
    }

    func_802282B8();
    func_80230B60();
    func_802345E0();
    func_80234770();

    if (D_801CFC70 == 0) {
        D_801CFC70 = 10;
    }
    if (D_801CFC70 >= 10) {
        D_801CFC70++;
        if (D_801CFC70 >= 41) {
            func_80178E30();
            D_801CFC70 = -1;
        }
    }
}
