/* Direct segments and TLB fallback, reconstructed from ROM 0x20B80. */
extern unsigned int os_virtual_to_physical_000254e0(void *address);

unsigned int os_virtual_to_physical(void *address)
{
    unsigned int physical;
    if ((unsigned int)address - 0x80000000U > 0x1FFFFFFFU) {
        if ((unsigned int)address - 0xA0000000U > 0x1FFFFFFFU)
            physical = os_virtual_to_physical_000254e0(address);
        else
            physical = (unsigned int)address & 0x1FFFFFFFU;
    } else
        physical = (unsigned int)address & 0x1FFFFFFFU;
    return physical;
}
