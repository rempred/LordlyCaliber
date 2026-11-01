typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned char u8;

extern u16 g_func_0015DF10_table[];
extern const u8 g_func_0015DF10_match[];
extern const u8 g_func_0015DF10_default[];
extern void func_000EA604(const void *argument);

void func_0015DF10(u32 value)
{
    register u32 zero asm("$0");
    register u32 index asm("$4");
    register u32 saved_value asm("$5");
    register u16 current asm("$2");
    register u16 *cursor asm("$3");

    saved_value = value + zero;
    asm volatile("addu %0,$0,$0" : "=r"(index));
    cursor = g_func_0015DF10_table;
    for (;;) {
        current = *cursor;
        index += 1;
        if (saved_value == current) {
            asm volatile(
                ".set noreorder\n"
                "lui $4,%%hi(g_func_0015DF10_match)\n"
                "j func_0020CDE4\n"
                "addiu $4,$4,%%lo(g_func_0015DF10_match)\n"
                ".set reorder"
                :
                :
            );
        }
        if (index < 15) {
            cursor += 1;
            continue;
        }
        break;
    }
    asm volatile(".set noreorder");
    func_000EA604(g_func_0015DF10_default);
    asm volatile("nop\n.set reorder" : : : "memory");
    asm volatile("" : : : "memory");
}
