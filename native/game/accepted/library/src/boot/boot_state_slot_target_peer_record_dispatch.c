extern void func_00008388(unsigned int slot);
extern volatile unsigned char g_state_slot_records[];

asm(".macro move dst,src\n"
    "addu \\dst,\\src,$0\n"
    ".endm\n");

void func_00007600(int target)
{
    register int target_reg asm("$18");
    register int slot asm("$16");
    register int offset asm("$17");
    register int zero asm("$0");

    target_reg = target + zero;
    if (target_reg < 0) {
        return;
    }
    slot = 0;
    offset = 0;
    do {
        if (slot != target_reg
            && ((*(volatile unsigned short *)(g_state_slot_records + offset) & 0x8000) != 0)
            && (*(signed short *)(g_state_slot_records + offset + 0xA2) == target_reg)) {
            func_00008388(slot + zero);
        }
        slot++;
        offset += 0xA8;
    } while (slot < 6);
}

asm(".size func_00007600, 136");
