extern unsigned char D_800AA450;
extern int func_00029fc0(void);
extern unsigned int osVirtualToPhysical(void *address);

int ai_set_next_buffer(void *buffer, unsigned int size)
{
    unsigned char *address;
    if (func_00029fc0())
        return -1;
    address = buffer;
    if (D_800AA450)
        address -= 0x2000;
    if ((((unsigned int)buffer + size) & 0x1FFF) == 0)
        D_800AA450 = 1;
    else
        D_800AA450 = 0;
    *(volatile unsigned int *)0xA4500000 = osVirtualToPhysical(address);
    *(volatile unsigned int *)0xA4500004 = size;
    return 0;
}
