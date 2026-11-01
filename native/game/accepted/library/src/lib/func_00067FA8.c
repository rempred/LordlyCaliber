typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

typedef struct {
    s16 terminal_class;
    s16 environment;
    s16 field_4;
    s16 field_6;
} Func00067FA8Launch __attribute__((packed));

extern Func00067FA8Launch D_8019A64C;
extern u16 D_8018F1A0;
extern void *D_8018FC14;
extern s8 D_80196AED;
extern u8 D_801976D9;

void func_80178E60(s32 selector, Func00067FA8Launch *launch,
                   Func00067FA8Launch *defaults, s32 selector_copy);
u32 func_8009DAF4(u32 resource);
void *func_80070F30(u32 size);
void *func_8009DBB8(void *destination, u32 resource);
void func_800712C4(void *pointer);
void *func_80198B04(u32 resource);
s32 func_80198360(u8 selector);
void func_801988A8(void);
void func_80178D10(void);

s32 func_00067fa8(void)
{
    Func00067FA8Launch launch;
    register u16 *request asm("$2") = &D_8018F1A0;
    register u32 selector asm("$7");
    register Func00067FA8Launch *defaults asm("$6");
    u32 request_flags;
    s32 environment;
    s32 terminal_class;
    u32 *table;
    u32 *group;

    selector = *request;
    defaults = &D_8019A64C;
    launch = *defaults;
    request_flags = *(volatile u16 *)request;
    D_8018FC14 = 0;
    selector &= 0x3FFF;
    if ((request_flags & 0x8000) == 0) {
        func_80178E60(selector, &launch, defaults, selector);
        environment = launch.environment;
        terminal_class = launch.terminal_class;
        if (environment >= -1) {
            if ((u32)(terminal_class - 4) < 2) {
                table = func_80070F30(func_8009DAF4(0x016B3D18));
                func_8009DBB8(table, 0x016B3D18);
                group = func_8009DBB8(0, table[environment]);
                func_800712C4(table);
                D_8018FC14 = func_80198B04(*group);
                func_800712C4(group);
            } else if (environment >= 0) {
                D_80196AED = environment;
                func_801988A8();
            } else {
                D_80196AED = func_80198360(D_801976D9) - 1;
                func_801988A8();
            }
        }
    } else {
        D_80196AED = func_80198360(D_801976D9) - 1;
        func_801988A8();
    }
    func_80178D10();
    return 0;
}

asm(".word 0\n.size func_00067fa8, .-func_00067fa8");
