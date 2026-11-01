typedef unsigned short StateSlotStatus;

extern void func_00008380(void);
extern void func_00008388(unsigned int slot);
extern volatile StateSlotStatus g_state_slot_status;
extern volatile unsigned char g_state_slot_records[];

void func_00007688(void)
{
    register unsigned int offset asm("$16");
    register int slot asm("$17");
    register int zero asm("$0");

    func_00008380();
    asm(".set noreorder");
    if (g_state_slot_status != 0xFFFF) {
        asm("addu %0,$0,$0" : "=r"(slot));
        asm("addu %0,$0,$0" : "=r"(offset));
        do {
            if ((*(volatile unsigned short *)(g_state_slot_records + offset) & 0x8000) != 0
                && ((*(volatile unsigned char *)(g_state_slot_records + offset + 3) & 0x04) != 0)) {
                func_00008388(slot + zero);
            }
            slot++;
            offset += 0xA8;
        } while (slot < 6);
    }
}

int func_00007714(unsigned int target)
{
    register int offset asm("$3");
    register int slot asm("$5");
    register int zero asm("$0");

    asm("addu %0,$0,$0" : "=r"(slot));
    asm("addu %0,$0,$0" : "=r"(offset));
    do {
        if ((*(volatile unsigned short *)(g_state_slot_records + offset) & 0x8000) != 0
            && *(volatile unsigned int *)(g_state_slot_records + offset + 0x10) == target) {
            return slot + zero;
        }
        slot++;
        offset += 0xA8;
    } while (slot < 6);
    return -1;
}

asm(".size func_00007688, 224");
