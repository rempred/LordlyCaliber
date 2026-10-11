typedef unsigned char u8;
typedef unsigned int u32;

extern void func_00089A10(void);
extern void func_0000368C(void);
extern void *func_000016C4(void *resource);

extern void *g_resource_state_pointer;
extern volatile u8 g_resource_state_byte_0;
extern volatile u8 g_resource_state_byte_1;
extern volatile u8 g_resource_state_byte_2;
extern volatile u8 g_resource_state_byte_3;
extern volatile u32 g_resource_state_word;

void func_00003798(void)
{
    void *resource;
    void *replacement;

    func_00089A10();
    func_0000368C();
    resource = g_resource_state_pointer;
    g_resource_state_byte_3 = 0;
    g_resource_state_byte_2 = 0;
    g_resource_state_byte_1 = 0;
    g_resource_state_byte_0 = 0;
    replacement = func_000016C4(resource);
    g_resource_state_pointer = replacement;
    g_resource_state_word = 0;
}
