#include "game/cutscene_panels.h"

extern struct {
    u8 field00;
    u8 pad01[5];
    s16 field06;
} data_00299e50;

extern u8 D_8023E5D0[];
extern u8 D_800EB0B0[];
extern void *D_8023DE38;
extern void *D_8018F1A4;
extern s16 D_8022A994;
extern s32 D_8022A990;

extern void bzero(void *buffer, s32 size);
extern void func_00047ee0(s32 selector, s32 index);
extern void func_0028aa4c(void);
extern void *func_00070F30(s32 size);
extern void *func_0002dfb8(void *buffer, u32 key);
extern void *func_0002e138(u32 key);
extern s32 resource_free(u8 *buffer);
extern void *func_8016FBE0(u32 field, s32 mode, void *output);
extern void *func_00291130(u8 **payloadEnd);
extern void func_800EA6B0(void *state, s32 arg1, s32 arg2);
extern void func_00283FA8(s32 scenario, s32 mode);
extern void func_00286620(void);
extern s32 func_00284288(void);

void func_0028adf4(void)
{
    u32 *keys;

    bzero(D_8023E5D0, 0x2F);
    data_00299e50.field06 = -1;
    data_00299e50.field00 = 0;
    func_00047ee0(0x43, 0);
    func_00047ee0(0x43, 1);
    func_00047ee0(0x43, 2);
    func_0028aa4c();

    D_8023DE34 = func_00070F30(sizeof(PanelParameters));
    bzero(D_8023DE34, sizeof(PanelParameters));
    keys = func_0002dfb8(0, 0x01890C90);
    D_8023DE34->field14 = func_0002e138(keys[19]);
    resource_free((u8 *)keys);

    D_8023DE34->field10 = func_8016FBE0(0x7D, 0, D_8023DE34);
    D_8023DE34->field08 = func_00291130(&D_8023DE34->field04);
    D_8023DE34->field0C = func_0002e138(0x01885534);
    func_800EA6B0(D_800EB0B0, 7, 5);
    D_8023DE38 = 0;

    D_8022A974 = func_00070F30(0x1CB8);
    bzero(D_8022A974, 0x1CB8);
    func_00283FA8(D_8022A994, 0);
    if (D_8018F1A4 != 0) {
        func_00286620();
    }
    D_8022A990 = 0;
    func_00284288();
}
