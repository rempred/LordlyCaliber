typedef unsigned short u16;
typedef unsigned int u32;

int func_000bfebc(void)
{
    int count;
    register u32 *record asm("$4");

    count = 0;
    asm volatile(
        "# Hybrid scope: bind the four-byte record cursor to retail's $a0.\n"
        "# lui writes 0x80190000; addiu adds 0x6B00, yielding 0x80196B00.\n"
        "# Only this address load is assembly; the scan, loop, and return are C.\n"
        "lui %0,0x8019 # load the upper half of the fixed record-table address into $a0\n"
        "addiu %0,%0,0x6B00 # complete record cursor address 0x80196B00 in $a0"
        : "=r"(record));
loop:
    if (*(u16 *)record != 0) {
        count++;
        record++;
        if (count < 0x116) {
            goto loop;
        }
    }
    return count & 0xFFFF;
}
