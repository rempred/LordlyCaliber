typedef unsigned short u16;

extern volatile u16 g_boot_state_slot_queue_service_gate;
extern void func_000775EC(void);
extern void func_00077BF8(void);
extern void func_0000859C(void);

void func_000071C8(void)
{
    if (g_boot_state_slot_queue_service_gate != 0) {
        func_000775EC();
        func_00077BF8();
        func_0000859C();
    }
}
