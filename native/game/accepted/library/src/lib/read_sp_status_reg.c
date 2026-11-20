typedef unsigned int u32;
typedef volatile u32 vu32;
u32 read_sp_status_reg(void) {
    return (*(vu32 *)((u32)(0x04040010) | 0xA0000000));
}
