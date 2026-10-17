extern int read_vi_current_field(void);
extern unsigned int osVirtualToPhysical(void *address);

int func_0002aa30(void *buffer, unsigned long long size)
{
    if (read_vi_current_field())
        return -1;
    *(volatile unsigned int *)0xA410000C = 1;
    do {
    } while (*(volatile unsigned int *)0xA410000C & 1);
    *(volatile unsigned int *)0xA4100000 = osVirtualToPhysical(buffer);
    *(volatile unsigned int *)0xA4100004 = osVirtualToPhysical(buffer) + size;
    return 0;
}
