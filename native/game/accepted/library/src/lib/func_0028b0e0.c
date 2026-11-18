#include "game/cutscene_panels.h"

extern void *D_8018FC08;
extern void *D_8018FC0C;

extern void func_00284184(void);
extern void func_0004ef58(void);
extern void func_0004ef18(void);
extern s32 resource_free(u8 *buffer);
extern void func_80079618(s32 command, ...);
extern void func_00291104(void);
extern void func_0028364C(void);

void func_0028b0e0(void)
{
    func_00283748(-1);
    func_00284184();
    func_0004ef58();
    func_0004ef18();

    resource_free((u8 *)D_8022A974);
    D_8022A974 = 0;

    resource_free(D_8023DE34->field14);
    resource_free(D_8023DE34->field10);
    resource_free(D_8023DE34->field08);
    resource_free(D_8023DE34->field0C);
    resource_free((u8 *)D_8023DE34);

    func_80079618(-0x11);
    func_00291104();
    func_0028364C();
    resource_free(D_8018FC08);
    resource_free(D_8018FC0C);
}
